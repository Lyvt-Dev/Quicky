#include "engine.hpp"
#include "qrcode.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QQuickWindow>
#include <QFileInfo>
#include <cstdio>
int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Lyvt");
    app.setApplicationName("Quicky");
    app.setApplicationVersion("0.1.0");
    QQuickStyle::setStyle("Basic");
    try {
        Engine service;
        service.start();
        QObject::connect(&service, &Engine::openFolderRequested, &app, [](const QString& path) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        UiBridge bridge;
        QQmlApplicationEngine qml;
        QObject::connect(&service, &Engine::finished, &bridge, [&bridge](const QString& path) {
            if (QFileInfo::exists(path)) bridge.setReceived(path);
        });
        QObject::connect(&service, &Engine::changed, &bridge, [&service, &bridge] {
            if (service.state() == "hashing" || service.state() == "offered") bridge.setReceived({});
        });
#ifdef Q_OS_ANDROID
        QObject::connect(&service, &Engine::changed, &app, [&service] {
            QNativeInterface::QAndroidApplication::context().callMethod<void>("keepAwake", "(Z)V", service.busy());
        });
#endif
        qml.addImageProvider("qr", new QrProvider);
        qml.rootContext()->setContextProperty("quicky", &service);
        qml.rootContext()->setContextProperty("ui", &bridge);
        qml.load(QUrl("qrc:/apps/native/Main.qml"));
        if (qml.rootObjects().isEmpty()) return 1;
        if (app.arguments().contains("--mobile")) {
            qml.rootObjects().first()->setProperty("width", 390);
            qml.rootObjects().first()->setProperty("height", 844);
        }
        const auto screenshotIndex = app.arguments().indexOf("--screenshot");
        if (screenshotIndex >= 0 && screenshotIndex + 1 < app.arguments().size()) {
            const auto path = app.arguments().at(screenshotIndex + 1);
            QTimer::singleShot(1000, &app, [&qml, &app, path] {
                auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().first());
                app.exit(window && window->grabWindow().save(path) ? 0 : 1);
            });
        }
        if (app.arguments().contains("--smoke-test")) QTimer::singleShot(1500, &app, &QCoreApplication::quit);
        return app.exec();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Quicky: %s\n", error.what());
        return 1;
    }
}
