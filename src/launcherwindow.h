#pragma once

#include "models/devicedefinitions.h"

#include <QMainWindow>

class QLabel;

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);

signals:
    void openSpectrometerRequested(const DeviceProfile &spectrometer);
    void openCameraRequested(const DeviceProfile &camera);

private:
    QLabel *m_summaryLabel = nullptr;

    void openSpectrometerDialog();
    void openCameraDialog();
};
