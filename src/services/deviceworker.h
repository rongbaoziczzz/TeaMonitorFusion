#pragma once

#include "models/devicedefinitions.h"

#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QScopedPointer>
#include <QVector>

class AbstractCameraService;
class AbstractSpectrometerService;
class QTimer;

class DeviceWorker : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Monitoring,
        Reconnecting,
        Fault
    };
    Q_ENUM(State)

    explicit DeviceWorker(const DeviceProfile &spectrometerProfile,
                          const DeviceProfile &cameraProfile,
                          QObject *parent = nullptr);
    ~DeviceWorker() override;

public slots:
    void initialize();
    void connectDevice();
    void disconnectDevice();
    void startMonitoring();
    void stopMonitoring();
    void setSpectrometerParameters(int integrationTimeUs, int smoothing, int averageCount);
    void setCameraParameters(int exposureMs, int gain);
    void shutdown();

signals:
    void stateChanged(DeviceWorker::State state, const QString &message);
    void spectrumReady(const QVector<double> &wavelengths,
                       const QVector<double> &intensities,
                       const QDateTime &capturedAt);
    void frameReady(const QImage &image, const QDateTime &capturedAt);
    void operationError(const QString &message);
    void parametersApplied();

private slots:
    void pollDevice();
    void attemptReconnect();

private:
    DeviceProfile m_spectrometerProfile;
    DeviceProfile m_cameraProfile;
    QScopedPointer<AbstractSpectrometerService> m_spectrometer;
    QScopedPointer<AbstractCameraService> m_camera;
    QTimer *m_pollTimer = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    State m_state = State::Disconnected;
    bool m_connectionRequested = false;
    bool m_monitoringRequested = false;
    bool m_initialized = false;
    int m_consecutiveFailures = 0;
    int m_integrationTimeUs = 100000;
    int m_smoothing = 5;
    int m_averageCount = 5;
    int m_exposureMs = 12;
    int m_gain = 1;
    QImage m_pendingCameraFrame;
    int m_reconnectAttempt = 0;

    bool isSpectrometerMode() const;
    bool isCameraMode() const;
    bool openService(QString *errorMessage);
    void closeService();
    bool applyParameters(QString *errorMessage);
    void updatePollingInterval();
    void setState(State state, const QString &message);
    void handleAcquisitionFailure(const QString &message);
    void scheduleReconnect();
    bool emitPendingCameraFrame();
};
