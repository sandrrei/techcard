#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariant>
#include <functional>

// Loads a CV and the public GitHub repos of its owner.
//
// The CV source is configured by the user and stored on the device:
//   - "url":   a JSON file on the web, refreshed on every start
//   - "local": JSON pasted or imported from a file, stored on the device
// Both the JSON Resume format (https://jsonresume.org) and the
// cv.srei.duckdns.org format are accepted and normalized to one map.
class DataStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap cv READ cv NOTIFY cvChanged)
    Q_PROPERTY(QVariantList repos READ repos NOTIFY reposChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString lastUpdated READ lastUpdated NOTIFY lastUpdatedChanged)
    Q_PROPERTY(QString avatarUrl READ avatarUrl NOTIFY avatarUrlChanged)
    Q_PROPERTY(QString cardUrl READ cardUrl NOTIFY cardUrlChanged)

    // Configuration
    Q_PROPERTY(bool configured READ configured NOTIFY configChanged)
    Q_PROPERTY(QString sourceMode READ sourceMode NOTIFY configChanged)
    Q_PROPERTY(QString sourceUrl READ sourceUrl NOTIFY configChanged)
    Q_PROPERTY(QString githubUser READ githubUser NOTIFY configChanged)
    Q_PROPERTY(QString pageUrl READ pageUrl NOTIFY configChanged)
    Q_PROPERTY(bool demo READ demo NOTIFY configChanged)

public:
    explicit DataStore(QObject *parent = nullptr);

    QVariantMap cv() const { return m_cv; }
    QVariantList repos() const { return m_repos; }
    bool loading() const { return m_pending > 0; }
    QString error() const { return m_error; }
    QString lastUpdated() const { return m_lastUpdated; }
    QString avatarUrl() const { return m_avatarUrl; }
    QString cardUrl() const;

    bool configured() const { return !m_sourceMode.isEmpty(); }
    QString sourceMode() const { return m_sourceMode; }
    QString sourceUrl() const { return m_sourceUrl; }
    QString githubUser() const { return m_githubUser; }
    QString pageUrl() const { return m_pageUrl; }
    bool demo() const { return m_demo; }

    Q_INVOKABLE void refresh();

    // Setup. Each returns an empty string on success or an error message.
    Q_INVOKABLE QString setSourceUrl(const QString &url, const QString &githubUser,
                                     const QString &pageUrl);
    Q_INVOKABLE QString importJson(const QString &json, const QString &githubUser,
                                   const QString &pageUrl);
    Q_INVOKABLE QString importFile(const QUrl &fileUrl, const QString &githubUser,
                                   const QString &pageUrl);
    Q_INVOKABLE void useDemo();
    Q_INVOKABLE void resetConfig();

    // vCard 3.0 built from the CV, for the "save contact" QR code
    Q_INVOKABLE QString vcard(bool includePhone) const;

signals:
    void cvChanged();
    void reposChanged();
    void loadingChanged();
    void errorChanged();
    void lastUpdatedChanged();
    void avatarUrlChanged();
    void configChanged();
    void cardUrlChanged();

private:
    void loadConfig();
    void saveConfig();
    void clearData();
    void fetch(const QUrl &url, const QString &cacheName,
               std::function<bool(const QByteArray &)> apply);
    void fetchRepos();
    QString effectiveGithubUser() const;
    bool applyCv(const QByteArray &json, QString *why = nullptr);
    bool applyRepos(const QByteArray &json);
    QString cachePath(const QString &name) const;
    void setError(const QString &error);
    void setLastUpdated(const QString &value);
    void setAvatarUrl(const QString &value);

    QNetworkAccessManager m_nam;
    QVariantMap m_cv;
    QVariantList m_repos;
    int m_pending = 0;
    bool m_reposRequested = false;
    int m_generation = 0; // bumped when the source changes; stale replies are ignored
    QList<QPointer<QNetworkReply>> m_replies;
    QString m_error;
    QString m_lastUpdated;
    QString m_avatarUrl;

    QString m_sourceMode; // "", "url" or "local"
    QString m_sourceUrl;
    QString m_githubUser;
    QString m_pageUrl;
    bool m_demo = false;
};
