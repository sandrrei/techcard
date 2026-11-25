#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>

#include "datastore.h"

#if defined(Q_OS_ANDROID) && defined(HAVE_ANDROIDEXTRAS)
#include <QAndroidJniObject>
#include <QtAndroid>

// true when Android's dark theme (night mode) is on
static bool androidNightMode()
{
    QAndroidJniObject activity = QtAndroid::androidActivity();
    if (!activity.isValid())
        return false;
    QAndroidJniObject resources =
        activity.callObjectMethod("getResources", "()Landroid/content/res/Resources;");
    QAndroidJniObject config =
        resources.callObjectMethod("getConfiguration", "()Landroid/content/res/Configuration;");
    if (!config.isValid())
        return false;
    const jint uiMode = config.getField<jint>("uiMode");
    return (uiMode & 0x30) == 0x20; // UI_MODE_NIGHT_MASK == UI_MODE_NIGHT_YES
}
#else
static bool androidNightMode() { return false; }
#endif
#include "qrcodeitem.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
#endif
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication app(argc, argv);

    // Must match the click app id so the cache lands in an AppArmor-allowed path
    QCoreApplication::setApplicationName(QStringLiteral("techcard.sandrrei"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.4.0"));

#ifdef Q_OS_ANDROID
    // Material theme is chosen once at start-up (Qt 5.15). Ubuntu Touch's Suru
    // style already follows the system theme on its own.
    const QString theme = DataStore::savedTheme();
    const bool dark = theme == QLatin1String("dark")
                      || (theme == QLatin1String("system") && androidNightMode());
    qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", dark ? "Dark" : "Light");
    qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", "#7C4DFF");
#endif

    qmlRegisterType<QrCodeItem>("TechCard", 1, 0, "QrCode");

    DataStore store;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("store"), &store);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    store.refresh();
    return app.exec();
}
