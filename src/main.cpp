#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtQml>

#include "datastore.h"
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
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.2"));

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
