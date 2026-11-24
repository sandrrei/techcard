#include "datastore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {
// Demo profile: a fictional CV bundled in the app (no personal data, works offline)
const QString DEMO_CV = QStringLiteral(":/demo/example-resume.json");
const QString DEMO_GITHUB = QStringLiteral("octocat"); // GitHub's public demo account

const QString CV_CACHE = QStringLiteral("cv.json");
const QString REPOS_CACHE = QStringLiteral("repos.json");
const QString LOCAL_CV = QStringLiteral("imported.json");

QString now()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QByteArray readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return f.write(data) == data.size();
}

QSettings &settings()
{
    // Explicit path: on Ubuntu Touch only ~/.config/<app id>/ is writable
    static QSettings s(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                           + QStringLiteral("/settings.ini"),
                       QSettings::IniFormat);
    return s;
}

// Escape text values as required by vCard 3.0 (RFC 2426)
QString vcEscape(QString s)
{
    s.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    s.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    s.replace(QLatin1Char(','), QStringLiteral("\\,"));
    s.replace(QLatin1Char(';'), QStringLiteral("\\;"));
    return s;
}

QString str(const QJsonObject &o, const char *key)
{
    return o.value(QLatin1String(key)).toString().trimmed();
}

QStringList strList(const QJsonValue &v)
{
    QStringList out;
    const QJsonArray arr = v.toArray();
    for (const QJsonValue &x : arr) {
        const QString s = x.toString().trimmed();
        if (!s.isEmpty())
            out << s;
    }
    return out;
}

// "2022-03-01" -> "2022"; empty -> fallback
QString year(const QString &date, const QString &fallback)
{
    return date.isEmpty() ? fallback : date.left(4);
}

// JSON Resume (https://jsonresume.org/schema) -> internal map
QVariantMap fromJsonResume(const QJsonObject &r)
{
    QVariantMap m;
    const QJsonObject b = r.value(QStringLiteral("basics")).toObject();

    m.insert(QStringLiteral("name"), str(b, "name"));
    m.insert(QStringLiteral("title"), str(b, "label"));
    m.insert(QStringLiteral("email"), str(b, "email"));
    m.insert(QStringLiteral("phone"), str(b, "phone"));
    m.insert(QStringLiteral("bio"), str(b, "summary"));
    m.insert(QStringLiteral("url"), str(b, "url"));
    m.insert(QStringLiteral("image"), str(b, "image"));

    const QJsonObject loc = b.value(QStringLiteral("location")).toObject();
    QStringList where;
    for (const char *k : {"city", "region", "countryCode"}) {
        const QString v = str(loc, k);
        if (!v.isEmpty())
            where << v;
    }
    m.insert(QStringLiteral("location"), where.join(QStringLiteral(", ")));

    const QJsonArray profiles = b.value(QStringLiteral("profiles")).toArray();
    for (const QJsonValue &pv : profiles) {
        const QJsonObject p = pv.toObject();
        const QString network = str(p, "network").toLower();
        QString url = str(p, "url");
        if (network.contains(QLatin1String("linkedin"))) {
            m.insert(QStringLiteral("linkedin"), url);
        } else if (network.contains(QLatin1String("github"))) {
            if (url.isEmpty() && !str(p, "username").isEmpty())
                url = QStringLiteral("https://github.com/") + str(p, "username");
            m.insert(QStringLiteral("github"), url);
        }
    }

    // skills: [{ name, keywords[] }] -> { name: [keywords] }
    QVariantMap skills;
    QStringList loose;
    const QJsonArray sk = r.value(QStringLiteral("skills")).toArray();
    for (const QJsonValue &sv : sk) {
        const QJsonObject s = sv.toObject();
        const QStringList kw = strList(s.value(QStringLiteral("keywords")));
        if (kw.isEmpty())
            loose << str(s, "name");
        else
            skills.insert(str(s, "name"), kw);
    }
    if (!loose.isEmpty())
        skills.insert(QStringLiteral("Skills"), loose);
    m.insert(QStringLiteral("skills"), skills);

    QVariantList work;
    const QJsonArray wk = r.value(QStringLiteral("work")).toArray();
    for (const QJsonValue &wv : wk) {
        const QJsonObject w = wv.toObject();
        QStringList highlights = strList(w.value(QStringLiteral("highlights")));
        if (highlights.isEmpty() && !str(w, "summary").isEmpty())
            highlights << str(w, "summary");
        QVariantMap e;
        e.insert(QStringLiteral("role"), str(w, "position"));
        e.insert(QStringLiteral("company"), str(w, "name"));
        e.insert(QStringLiteral("period"),
                 year(str(w, "startDate"), QStringLiteral("?")) + QStringLiteral(" - ")
                     + year(str(w, "endDate"), QStringLiteral("Present")));
        e.insert(QStringLiteral("highlights"), highlights);
        work << e;
    }
    m.insert(QStringLiteral("experience"), work);

    QVariantList projects;
    const QJsonArray pj = r.value(QStringLiteral("projects")).toArray();
    for (const QJsonValue &pv : pj) {
        const QJsonObject p = pv.toObject();
        QVariantMap e;
        e.insert(QStringLiteral("name"), str(p, "name"));
        e.insert(QStringLiteral("description"), str(p, "description"));
        e.insert(QStringLiteral("tech"),
                 strList(p.value(QStringLiteral("keywords"))).join(QStringLiteral(", ")));
        e.insert(QStringLiteral("url"), str(p, "url"));
        projects << e;
    }
    m.insert(QStringLiteral("projects"), projects);
    return m;
}
} // namespace

DataStore::DataStore(QObject *parent)
    : QObject(parent)
{
    loadConfig();
    if (!configured())
        return;

    // Show the stored card immediately, then refresh from the network
    const QString cvFile = cachePath(m_sourceMode == QLatin1String("local") ? LOCAL_CV : CV_CACHE);
    const QByteArray cv = readFile(cvFile);
    if (!cv.isEmpty() && applyCv(cv))
        setLastUpdated(QFileInfo(cvFile).lastModified()
                           .toString(QStringLiteral("yyyy-MM-dd HH:mm")));

    const QByteArray repos = readFile(cachePath(REPOS_CACHE));
    if (!repos.isEmpty())
        applyRepos(repos);
}

// ---------------------------------------------------------------- config

void DataStore::loadConfig()
{
    QSettings &s = settings();
    m_sourceMode = s.value(QStringLiteral("source/mode")).toString();
    m_sourceUrl = s.value(QStringLiteral("source/url")).toString();
    m_githubUser = s.value(QStringLiteral("github/user")).toString();
    m_pageUrl = s.value(QStringLiteral("share/pageUrl")).toString();
    m_demo = s.value(QStringLiteral("source/demo"), false).toBool();
}

void DataStore::saveConfig()
{
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    QSettings &s = settings();
    s.setValue(QStringLiteral("source/mode"), m_sourceMode);
    s.setValue(QStringLiteral("source/url"), m_sourceUrl);
    s.setValue(QStringLiteral("github/user"), m_githubUser);
    s.setValue(QStringLiteral("share/pageUrl"), m_pageUrl);
    s.setValue(QStringLiteral("source/demo"), m_demo);
    s.sync();
    emit configChanged();
    emit cardUrlChanged();
}

void DataStore::clearData()
{
    // Drop requests for the previous source
    ++m_generation;
    const auto replies = m_replies;
    m_replies.clear();
    for (const QPointer<QNetworkReply> &r : replies)
        if (r)
            r->abort();

    QFile::remove(cachePath(CV_CACHE));
    QFile::remove(cachePath(REPOS_CACHE));
    QFile::remove(cachePath(LOCAL_CV));
    m_cv.clear();
    m_repos.clear();
    emit cvChanged();
    emit reposChanged();
    setAvatarUrl(QString());
    setLastUpdated(QString());
    setError(QString());
}

static QString cleanGithubUser(QString user)
{
    // Accept "name", "@name" or "https://github.com/name"
    user = user.trimmed();
    user.remove(QRegularExpression(QStringLiteral("^https?://(www\\.)?github\\.com/")));
    user.remove(QLatin1Char('@'));
    return user.section(QLatin1Char('/'), 0, 0);
}

QString DataStore::setSourceUrl(const QString &url, const QString &githubUser,
                                const QString &pageUrl)
{
    const QUrl u = QUrl::fromUserInput(url.trimmed());
    if (!u.isValid() || (u.scheme() != QLatin1String("https") && u.scheme() != QLatin1String("http")))
        return tr("Enter a valid http(s) link to a JSON file");

    clearData();
    m_sourceMode = QStringLiteral("url");
    m_sourceUrl = u.toString();
    m_githubUser = cleanGithubUser(githubUser);
    m_pageUrl = pageUrl.trimmed();
    m_demo = false;
    saveConfig();
    refresh();
    return QString();
}

QString DataStore::importJson(const QString &json, const QString &githubUser,
                              const QString &pageUrl)
{
    const QByteArray data = json.trimmed().toUtf8();
    QString why;
    QVariantMap backup = m_cv;
    if (!applyCv(data, &why)) {
        m_cv = backup;
        return why;
    }
    const QVariantMap imported = m_cv;

    clearData();
    if (!writeFile(cachePath(LOCAL_CV), data))
        return tr("Could not save the CV on this device");

    m_cv = imported;
    emit cvChanged();
    setLastUpdated(now());

    m_sourceMode = QStringLiteral("local");
    m_sourceUrl.clear();
    m_githubUser = cleanGithubUser(githubUser);
    m_pageUrl = pageUrl.trimmed();
    m_demo = false;
    saveConfig();
    refresh();
    return QString();
}

QString DataStore::importFile(const QUrl &fileUrl, const QString &githubUser,
                              const QString &pageUrl)
{
    // Local files and Android content:// URIs (QFile handles both in Qt >= 5.13)
    QFile f(fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString());
    if (!f.open(QIODevice::ReadOnly))
        return tr("Could not open the file");
    return importJson(QString::fromUtf8(f.readAll()), githubUser, pageUrl);
}

void DataStore::useDemo()
{
    if (!importJson(QString::fromUtf8(readFile(DEMO_CV)), DEMO_GITHUB, QString()).isEmpty())
        return;
    m_demo = true;
    saveConfig();
}

void DataStore::resetConfig()
{
    clearData();
    m_sourceMode.clear();
    m_sourceUrl.clear();
    m_githubUser.clear();
    m_pageUrl.clear();
    m_demo = false;
    saveConfig();
}

QString DataStore::cardUrl() const
{
    // Page recruiters open from the "CV link" QR code
    if (!m_pageUrl.isEmpty())
        return m_pageUrl;
    const QString url = m_cv.value(QStringLiteral("url")).toString();
    if (!url.isEmpty())
        return url;
    return m_cv.value(QStringLiteral("linkedin")).toString();
}

QString DataStore::effectiveGithubUser() const
{
    if (!m_githubUser.isEmpty())
        return m_githubUser;
    // Fall back to the GitHub link in the CV
    return cleanGithubUser(m_cv.value(QStringLiteral("github")).toString());
}

// ---------------------------------------------------------------- network

QString DataStore::cachePath(const QString &name) const
{
    // Ubuntu Touch: ~/.local/share/techcard.sandrrei (allowed by AppArmor)
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + name;
}

void DataStore::refresh()
{
    if (!configured() || m_pending > 0)
        return;
    setError(QString());
    m_reposRequested = false;

    if (m_sourceMode == QLatin1String("url")) {
        fetch(QUrl(m_sourceUrl), CV_CACHE, [this](const QByteArray &d) {
            const bool ok = applyCv(d);
            if (ok) {
                setLastUpdated(now());
                fetchRepos(); // the CV may carry the GitHub link
            }
            return ok;
        });
    }
    fetchRepos();
}

void DataStore::fetchRepos()
{
    const QString user = effectiveGithubUser();
    if (m_reposRequested || user.isEmpty())
        return;
    m_reposRequested = true;

    const QUrl url(QStringLiteral("https://api.github.com/users/%1/repos?per_page=100&sort=pushed")
                       .arg(user));
    fetch(url, REPOS_CACHE, [this](const QByteArray &d) { return applyRepos(d); });
}

void DataStore::fetch(const QUrl &url, const QString &cacheName,
                      std::function<bool(const QByteArray &)> apply)
{
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "TechCard/0.2"); // GitHub rejects requests without it
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    if (m_pending++ == 0)
        emit loadingChanged();

    QNetworkReply *reply = m_nam.get(req);
    m_replies.append(reply);
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, cacheName, apply, generation]() {
        reply->deleteLater();
        m_replies.removeAll(reply);

        if (generation != m_generation) {
            // Reply for a source that was replaced in the meantime
        } else if (reply->error() != QNetworkReply::NoError) {
            setError(tr("%1: %2").arg(reply->url().host(), reply->errorString()));
        } else {
            const QByteArray data = reply->readAll();
            if (apply(data))
                writeFile(cachePath(cacheName), data);
            else
                setError(tr("%1: not a valid CV / JSON").arg(reply->url().host()));
        }

        if (--m_pending == 0)
            emit loadingChanged();
    });
}

// ---------------------------------------------------------------- parsing

bool DataStore::applyCv(const QByteArray &json, QString *why)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (why)
            *why = tr("Invalid JSON: %1").arg(err.errorString());
        return false;
    }

    // cv.srei.duckdns.org wraps the CV as { success, data: {...} }
    const QJsonObject root = doc.object();
    QJsonObject data = root.value(QStringLiteral("data")).isObject()
                           ? root.value(QStringLiteral("data")).toObject()
                           : root;

    QVariantMap cv;
    if (data.contains(QStringLiteral("basics"))) {
        cv = fromJsonResume(data);
    } else {
        data.remove(QStringLiteral("_locals"));
        cv = data.toVariantMap();
    }

    if (cv.value(QStringLiteral("name")).toString().isEmpty()) {
        if (why)
            *why = tr("This JSON has no name: use the JSON Resume format (basics.name)");
        return false;
    }

    m_cv = cv;
    emit cvChanged();
    emit cardUrlChanged();
    if (m_avatarUrl.isEmpty() && !m_cv.value(QStringLiteral("image")).toString().isEmpty())
        setAvatarUrl(m_cv.value(QStringLiteral("image")).toString());
    return true;
}

bool DataStore::applyRepos(const QByteArray &json)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return false;

    QVariantList list;
    QString avatar;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        const QJsonObject r = v.toObject();
        // Every repo carries the owner's avatar: no extra API call needed
        if (avatar.isEmpty())
            avatar = r.value(QStringLiteral("owner")).toObject()
                         .value(QStringLiteral("avatar_url")).toString();
        if (r.value(QStringLiteral("fork")).toBool() || r.value(QStringLiteral("archived")).toBool())
            continue;

        QVariantMap m;
        m.insert(QStringLiteral("name"), r.value(QStringLiteral("name")).toString());
        m.insert(QStringLiteral("description"), r.value(QStringLiteral("description")).toString());
        m.insert(QStringLiteral("language"), r.value(QStringLiteral("language")).toString());
        m.insert(QStringLiteral("stars"), r.value(QStringLiteral("stargazers_count")).toInt());
        m.insert(QStringLiteral("url"), r.value(QStringLiteral("html_url")).toString());
        m.insert(QStringLiteral("pushed"), r.value(QStringLiteral("pushed_at")).toString().left(10));
        list.append(m);
    }

    m_repos = list;
    emit reposChanged();
    if (!avatar.isEmpty())
        setAvatarUrl(avatar);
    return true;
}

// ---------------------------------------------------------------- vCard

QString DataStore::vcard(bool includePhone) const
{
    const QString name = m_cv.value(QStringLiteral("name")).toString().simplified();
    if (name.isEmpty())
        return QString();

    // "Sandro Rei" -> family "Rei", given "Sandro"
    const QStringList parts = name.split(QLatin1Char(' '));
    const QString family = parts.last();
    const QString given = parts.size() > 1 ? parts.mid(0, parts.size() - 1).join(QLatin1Char(' '))
                                           : QString();

    QStringList v;
    v << QStringLiteral("BEGIN:VCARD")
      << QStringLiteral("VERSION:3.0")
      << QStringLiteral("N:%1;%2;;;").arg(vcEscape(family), vcEscape(given))
      << QStringLiteral("FN:%1").arg(vcEscape(name));

    const QString title = m_cv.value(QStringLiteral("title")).toString();
    if (!title.isEmpty())
        v << QStringLiteral("TITLE:%1").arg(vcEscape(title));

    const QString email = m_cv.value(QStringLiteral("email")).toString();
    if (!email.isEmpty())
        v << QStringLiteral("EMAIL;TYPE=INTERNET:%1").arg(email);

    if (includePhone) {
        QString phone = m_cv.value(QStringLiteral("phone")).toString();
        phone.remove(QRegularExpression(QStringLiteral("[^+0-9]")));
        if (!phone.isEmpty())
            v << QStringLiteral("TEL;TYPE=CELL:%1").arg(phone);
    }

    const QString linkedin = m_cv.value(QStringLiteral("linkedin")).toString();
    if (!linkedin.isEmpty())
        v << QStringLiteral("URL:%1").arg(linkedin);
    const QString page = cardUrl();
    if (!page.isEmpty() && page != linkedin)
        v << QStringLiteral("URL:%1").arg(page);

    v << QStringLiteral("END:VCARD");
    return v.join(QStringLiteral("\r\n"));
}

// ---------------------------------------------------------------- setters

void DataStore::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void DataStore::setLastUpdated(const QString &value)
{
    if (m_lastUpdated == value)
        return;
    m_lastUpdated = value;
    emit lastUpdatedChanged();
}

void DataStore::setAvatarUrl(const QString &value)
{
    if (m_avatarUrl == value)
        return;
    m_avatarUrl = value;
    emit avatarUrlChanged();
}
