#pragma once

#include "models/devicedefinitions.h"

#include <QMainWindow>

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);

signals:
    void openSpectrometerRequested(const DeviceProfile &spectrometer);
    void openCameraRequested(const DeviceProfile &camera);
    void openFusionRequested(const DeviceProfile &spectrometer, const DeviceProfile &camera);

private:
    void openSpectrometerDialog();
    void openCameraDialog();
    void openFusionDialogs();
    void openHistoryDialog();
};
