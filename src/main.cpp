#include "launcherwindow.h"
#include "mainwindow.h"
#include "models/devicedefinitions.h"
#include "ui/apptheme.h"

#include <QApplication>
#include <QIcon>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (HWND consoleWindow = GetConsoleWindow()) {
        ShowWindow(consoleWindow, SW_HIDE);
        FreeConsole();
    }
#endif

    QApplication app(argc, argv);
    app.setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("TeaMonitorFusion"));
    app.setStyleSheet(AppTheme::applicationStyleSheet());
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.ico")));

    LauncherWindow launcher;
    launcher.setWindowIcon(app.windowIcon());
    launcher.show();

    const DeviceProfile noSpectrometer{
        QStringLiteral("none"),
        QStringLiteral("未启用"),
        QString(),
        QStringLiteral("-"),
        QStringLiteral("-"),
        true};

    const DeviceProfile noCamera{
        QStringLiteral("none"),
        QStringLiteral("未启用"),
        QString(),
        QStringLiteral("-"),
        QStringLiteral("-"),
        true};

    auto openWindow = [&launcher, &app](const DeviceProfile &spectrometer, const DeviceProfile &camera) {
        auto *window = new MainWindow(spectrometer, camera);
        window->setAttribute(Qt::WA_DeleteOnClose, true);
        window->setWindowIcon(app.windowIcon());
        window->show();
        launcher.hide();

        QObject::connect(window, &MainWindow::requestReturnToLauncher, &launcher, [&launcher]() {
            launcher.show();
            launcher.raise();
            launcher.activateWindow();
        });

        QObject::connect(window, &QObject::destroyed, &launcher, [&launcher]() {
            launcher.show();
            launcher.raise();
            launcher.activateWindow();
        });
    };

    QObject::connect(&launcher, &LauncherWindow::openSpectrometerRequested, &launcher, [&](const DeviceProfile &spectrometer) {
        openWindow(spectrometer, noCamera);
    });

    QObject::connect(&launcher, &LauncherWindow::openCameraRequested, &launcher, [&](const DeviceProfile &camera) {
        openWindow(noSpectrometer, camera);
    });

    return app.exec();
}
