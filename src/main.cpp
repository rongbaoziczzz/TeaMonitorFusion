#include "launcherwindow.h"
#include "mainwindow.h"
#include "models/devicedefinitions.h"
#include "services/auditstore.h"
#include "services/filelogger.h"
#include "ui/apptheme.h"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>

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
    app.setOrganizationName(QStringLiteral("TeaMonitorFusion"));
    app.setOrganizationDomain(QStringLiteral("local.teaoem"));
    app.setStyle(QStringLiteral("Fusion"));
    app.setApplicationName(QStringLiteral("TeaMonitorFusion"));
    app.setApplicationVersion(QStringLiteral(TEA_MONITOR_VERSION));
    app.setStyleSheet(AppTheme::applicationStyleSheet());
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.ico")));

    const QString lockDirectory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(lockDirectory);
    QLockFile instanceLock(QDir(lockDirectory).filePath(QStringLiteral("TeaMonitorFusion.lock")));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(100)) {
        QMessageBox::warning(nullptr,
                             QStringLiteral("程序已在运行"),
                             QStringLiteral("TeaMonitorFusion 已经打开，请先关闭已有窗口后再启动。"));
        return 2;
    }

    FileLogger::initialize();
    FileLogger::write(QStringLiteral("应用版本：%1").arg(app.applicationVersion()));
    QString auditError;
    if (!AuditStore::initialize(QString(), &auditError)) {
        qWarning().noquote() << QStringLiteral("审计数据库初始化失败：%1").arg(auditError);
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        AuditStore::shutdown();
        FileLogger::shutdown();
    });

    LauncherWindow launcher;
    launcher.setWindowIcon(app.windowIcon());

    const DeviceProfile noSpectrometer{
        QStringLiteral("none"),
        QStringLiteral("本次不启用光谱仪"),
        QString(),
        QStringLiteral("-"),
        QStringLiteral("-"),
        true,
        true};

    const DeviceProfile noCamera{
        QStringLiteral("none"),
        QStringLiteral("本次不启用工业相机"),
        QString(),
        QStringLiteral("-"),
        QStringLiteral("-"),
        true,
        true};

    auto openWindow = [&launcher, &app](const DeviceProfile &spectrometer, const DeviceProfile &camera) -> MainWindow * {
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
        return window;
    };

    QObject::connect(&launcher, &LauncherWindow::openSpectrometerRequested, &launcher, [&](const DeviceProfile &spectrometer) {
        openWindow(spectrometer, noCamera);
    });

    QObject::connect(&launcher, &LauncherWindow::openCameraRequested, &launcher, [&](const DeviceProfile &camera) {
        openWindow(noSpectrometer, camera);
    });

    QObject::connect(&launcher, &LauncherWindow::openFusionRequested, &launcher,
                     [&](const DeviceProfile &spectrometer, const DeviceProfile &camera) {
                         openWindow(spectrometer, camera);
                     });

    const bool demoMode = app.arguments().contains(QStringLiteral("--demo"));
    const bool cameraLiveMode = app.arguments().contains(QStringLiteral("--camera-live"));
    if (demoMode) {
        const DeviceProfile demoSpectrometer{
            QStringLiteral("sim-spec"),
            QStringLiteral("离线光谱数据源"),
            QStringLiteral("提供标准光谱数据。"),
            QStringLiteral("420-804 nm"),
            QStringLiteral("内置数据源"),
            true,
            true};
        const DeviceProfile demoCamera{
            QStringLiteral("sim-camera"),
            QStringLiteral("离线图像数据源"),
            QStringLiteral("提供连续图像数据。"),
            QStringLiteral("-"),
            QStringLiteral("内置数据源"),
            true,
            true};
        openWindow(demoSpectrometer, demoCamera);
    } else if (cameraLiveMode) {
        const QString recoveryMarker = QDir(QCoreApplication::applicationDirPath())
                                           .filePath(QStringLiteral("../diagnostics/camera-recovery-admin.exit"));
        QFile markerFile(QDir::cleanPath(recoveryMarker));
        const QFileInfo markerInfo(markerFile);
        const bool recoveryReady = markerFile.exists() &&
                                    markerInfo.lastModified().date() == QDate::currentDate() &&
                                    markerFile.open(QIODevice::ReadOnly | QIODevice::Text) &&
                                    QString::fromUtf8(markerFile.readLine()).trimmed() == QStringLiteral("0");
        if (!recoveryReady) {
            qCritical().noquote() << QStringLiteral(
                "当前未确认今天的相机网卡恢复，请通过 scripts/run-camera-live.bat 启动。标记文件：%1")
                                         .arg(QDir::cleanPath(recoveryMarker));
            QMessageBox::critical(nullptr,
                                  QStringLiteral("相机网络尚未准备"),
                                  QStringLiteral("请先关闭当前程序，然后双击 scripts/run-camera-live.bat 完成网卡恢复。"));
            return 3;
        }
        DeviceProfile cameraProfile;
        for (const auto &profile : availableCameras()) {
            if (profile.id == QStringLiteral("aravis-camera")) {
                cameraProfile = profile;
                break;
            }
        }
        auto *window = openWindow(noSpectrometer, cameraProfile);
        QTimer::singleShot(0, window, &MainWindow::startCameraLivePreview);
    } else {
        launcher.show();
    }

    return app.exec();
}
