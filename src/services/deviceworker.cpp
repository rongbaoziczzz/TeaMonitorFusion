#include "deviceworker.h"

#include "abstractcameraservice.h"
#include "abstractspectrometerservice.h"
#include "araviscameraservice.h"
#include "mockcameraservice.h"
#include "mockspectrometerservice.h"
#include "oceandirectspectrometerservice.h"
#include "seaspectrometerservice.h"

#include <QElapsedTimer>
#include <QTimer>

DeviceWorker::DeviceWorker(const DeviceProfile &spectrometerProfile,
                           const DeviceProfile &cameraProfile,
                           QObject *parent)
    : QObject(parent)
    , m_spectrometerProfile(spectrometerProfile)
    , m_cameraProfile(cameraProfile)
{
}

DeviceWorker::~DeviceWorker()
{
    closeService();
}

void DeviceWorker::initialize()
{
    if (m_initialized) {
        return;
    }

    if (m_spectrometerProfile.id == QStringLiteral("ocean-direct")) {
        m_spectrometer.reset(new OceanDirectSpectrometerService());
    } else if (m_spectrometerProfile.id == QStringLiteral("oceanhood")) {
        m_spectrometer.reset(new SeaSpectrometerService());
    } else if (m_spectrometerProfile.id == QStringLiteral("sim-spec")) {
        m_spectrometer.reset(new MockSpectrometerService());
    }

    if (m_cameraProfile.id == QStringLiteral("aravis-camera")) {
        m_camera.reset(new AravisCameraService(m_cameraProfile.connectionId,
                                               m_cameraProfile.networkAddress,
                                               m_cameraProfile.interfaceAddress));
    } else if (m_cameraProfile.id == QStringLiteral("sim-camera")) {
        m_camera.reset(new MockCameraService());
    }

    m_pollTimer = new QTimer(this);
    m_pollTimer->setTimerType(Qt::PreciseTimer);
    m_pollTimer->setSingleShot(true);
    connect(m_pollTimer, &QTimer::timeout, this, &DeviceWorker::pollDevice);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer, &QTimer::timeout, this, &DeviceWorker::attemptReconnect);

    updatePollingInterval();
    m_initialized = true;
    setState(State::Disconnected, QStringLiteral("等待连接"));
}

bool DeviceWorker::isSpectrometerMode() const
{
    return static_cast<bool>(m_spectrometer) && !m_camera;
}

bool DeviceWorker::isCameraMode() const
{
    return static_cast<bool>(m_camera) && !m_spectrometer;
}

void DeviceWorker::setState(State state, const QString &message)
{
    m_state = state;
    emit stateChanged(state, message);
}

bool DeviceWorker::openService(QString *errorMessage)
{
    if (m_spectrometer) {
        if (!m_spectrometer->open(errorMessage)) {
            return false;
        }
        if (!applyParameters(errorMessage)) {
            closeService();
            return false;
        }
    } else if (m_camera) {
        if (!m_camera->setParameters(m_exposureMs, m_gain, errorMessage)) {
            return false;
        }
        if (!m_camera->open(errorMessage)) {
            return false;
        }
        QElapsedTimer firstFrameWait;
        firstFrameWait.start();
        QString lastFrameError;
        const qint64 firstFrameWaitLimitMs = 20000;
        while (firstFrameWait.elapsed() < firstFrameWaitLimitMs) {
            QString frameError;
            const QImage firstFrame = m_camera->grabFrame(&frameError);
            if (!firstFrame.isNull()) {
                m_pendingCameraFrame = firstFrame;
                return true;
            }
            if (!frameError.isEmpty()) {
                lastFrameError = frameError;
            }
            const int waitedSeconds = static_cast<int>(firstFrameWait.elapsed() / 1000);
            setState(m_state == State::Reconnecting ? State::Reconnecting : State::Connecting,
                     QStringLiteral("控制连接已建立，正在等待相机首帧（已等待 %1 秒）").arg(waitedSeconds));
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("等待相机首帧超过 %1 秒。%2")
                                .arg(firstFrameWaitLimitMs / 1000)
                                .arg(lastFrameError);
        }
        closeService();
        return false;
    } else {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未选择可用设备。");
        }
        return false;
    }

    return true;
}

void DeviceWorker::closeService()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    if (m_spectrometer) {
        m_spectrometer->close();
    }
    if (m_camera) {
        m_camera->close();
    }
    m_pendingCameraFrame = {};
}

void DeviceWorker::connectDevice()
{
    if (!m_initialized) {
        initialize();
    }
    if (m_state == State::Connected || m_state == State::Monitoring) {
        return;
    }

    m_connectionRequested = true;
    m_reconnectTimer->stop();
    m_reconnectAttempt = 0;
    setState(State::Connecting, QStringLiteral("正在连接设备"));

    QString errorMessage;
    if (!openService(&errorMessage)) {
        setState(State::Reconnecting, QStringLiteral("连接未完成，正在等待设备图像流"));
        emit operationError(errorMessage.isEmpty() ? QStringLiteral("设备连接失败。") : errorMessage);
        scheduleReconnect();
        return;
    }

    m_consecutiveFailures = 0;
    const QString summary = m_spectrometer ? m_spectrometer->deviceSummary() : m_camera->deviceSummary();
    setState(State::Connected, summary);
    emitPendingCameraFrame();
}

void DeviceWorker::disconnectDevice()
{
    m_connectionRequested = false;
    m_monitoringRequested = false;
    m_reconnectAttempt = 0;
    m_consecutiveFailures = 0;
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    closeService();
    setState(State::Disconnected, QStringLiteral("设备已断开"));
}

void DeviceWorker::startMonitoring()
{
    const bool open = (m_spectrometer && m_spectrometer->isOpen()) || (m_camera && m_camera->isOpen());
    if (!open) {
        emit operationError(QStringLiteral("设备尚未连接，无法开始检测。"));
        return;
    }

    QString errorMessage;
    if (m_spectrometer && !applyParameters(&errorMessage)) {
        emit operationError(errorMessage);
        setState(State::Fault, errorMessage);
        return;
    }

    m_monitoringRequested = true;
    m_consecutiveFailures = 0;
    updatePollingInterval();
    setState(State::Monitoring, QStringLiteral("检测中"));
    QTimer::singleShot(0, this, &DeviceWorker::pollDevice);
}

void DeviceWorker::stopMonitoring()
{
    m_monitoringRequested = false;
    if (m_pollTimer) {
        m_pollTimer->stop();
    }
    const bool open = (m_spectrometer && m_spectrometer->isOpen()) || (m_camera && m_camera->isOpen());
    setState(open ? State::Connected : State::Disconnected,
             open ? QStringLiteral("检测已暂停") : QStringLiteral("设备未连接"));
}

bool DeviceWorker::applyParameters(QString *errorMessage)
{
    if (m_spectrometer) {
        return m_spectrometer->setIntegrationTimeUs(m_integrationTimeUs, errorMessage) &&
               m_spectrometer->setSmoothing(m_smoothing, errorMessage) &&
               m_spectrometer->setAverageCount(m_averageCount, errorMessage);
    }
    if (m_camera) {
        return m_camera->setParameters(m_exposureMs, m_gain, errorMessage);
    }
    return false;
}

void DeviceWorker::setSpectrometerParameters(int integrationTimeUs, int smoothing, int averageCount)
{
    m_integrationTimeUs = integrationTimeUs;
    m_smoothing = smoothing;
    m_averageCount = averageCount;
    updatePollingInterval();

    QString errorMessage;
    if (!applyParameters(&errorMessage)) {
        emit operationError(errorMessage);
        return;
    }
    emit parametersApplied();
}

void DeviceWorker::setCameraParameters(int exposureMs, int gain)
{
    m_exposureMs = exposureMs;
    m_gain = gain;

    QString errorMessage;
    if (!applyParameters(&errorMessage)) {
        emit operationError(errorMessage);
        return;
    }
    emit parametersApplied();
}

void DeviceWorker::updatePollingInterval()
{
    if (!m_pollTimer) {
        return;
    }
    if (isSpectrometerMode()) {
        const qint64 expectedMs = (static_cast<qint64>(m_integrationTimeUs) * m_averageCount) / 1000;
        m_pollTimer->setInterval(static_cast<int>(qBound<qint64>(qint64(250), expectedMs + 100, qint64(60000))));
    } else {
        m_pollTimer->setInterval(30);
    }
}

void DeviceWorker::pollDevice()
{
    if (m_state != State::Monitoring) {
        return;
    }

    QString errorMessage;
    if (isSpectrometerMode()) {
        QVector<double> wavelengths;
        QVector<double> intensities;
        if (!m_spectrometer->acquire(wavelengths, intensities, &errorMessage) ||
            wavelengths.isEmpty() || wavelengths.size() != intensities.size()) {
            handleAcquisitionFailure(errorMessage.isEmpty() ? QStringLiteral("光谱数据长度无效。") : errorMessage);
            return;
        }
        m_consecutiveFailures = 0;
        emit spectrumReady(wavelengths, intensities, QDateTime::currentDateTime());
        if (m_state == State::Monitoring) {
            m_pollTimer->start();
        }
        return;
    }

    if (isCameraMode()) {
        const QImage image = m_camera->grabFrame(&errorMessage);
        if (image.isNull()) {
            handleAcquisitionFailure(errorMessage.isEmpty() ? QStringLiteral("相机未返回有效图像。") : errorMessage);
            return;
        }
        m_consecutiveFailures = 0;
        emit frameReady(image, QDateTime::currentDateTime());
        if (m_state == State::Monitoring) {
            m_pollTimer->start();
        }
    }
}

void DeviceWorker::handleAcquisitionFailure(const QString &message)
{
    ++m_consecutiveFailures;
    const int failureLimit = isCameraMode() ? 6 : 3;
    if (!isCameraMode() || m_consecutiveFailures == 1 || m_consecutiveFailures >= failureLimit) {
        emit operationError(QStringLiteral("采集失败（%1/%2）：%3")
                                .arg(m_consecutiveFailures)
                                .arg(failureLimit)
                                .arg(message));
    }
    if (m_consecutiveFailures < failureLimit) {
        m_pollTimer->start();
        return;
    }

    if (isCameraMode()) {
        setState(State::Reconnecting, QStringLiteral("图像流中断，正在重建数据通道"));
        QString recoveryError;
        if (m_camera->recoverStream(&recoveryError)) {
            const QImage firstFrame = m_camera->grabFrame(&recoveryError);
            if (!firstFrame.isNull()) {
                m_consecutiveFailures = 0;
                m_reconnectAttempt = 0;
                m_pendingCameraFrame = firstFrame;
                setState(State::Monitoring, QStringLiteral("图像流已恢复，检测继续"));
                emitPendingCameraFrame();
                m_pollTimer->start();
                return;
            }
        }
        emit operationError(QStringLiteral("图像流局部恢复失败：%1")
                                .arg(recoveryError.isEmpty() ? QStringLiteral("未收到首帧") : recoveryError));
    }
    closeService();
    setState(State::Reconnecting, QStringLiteral("设备通信中断，正在重新连接"));
    scheduleReconnect();
}

void DeviceWorker::attemptReconnect()
{
    if (!m_connectionRequested) {
        return;
    }

    setState(State::Reconnecting, QStringLiteral("正在重新连接设备"));
    QString errorMessage;
    if (!openService(&errorMessage)) {
        emit operationError(QStringLiteral("自动重连失败：%1").arg(errorMessage));
        scheduleReconnect();
        return;
    }

    m_consecutiveFailures = 0;
    m_reconnectAttempt = 0;
    emitPendingCameraFrame();
    if (m_monitoringRequested) {
        updatePollingInterval();
        setState(State::Monitoring, QStringLiteral("设备已重连，首帧已确认，检测已恢复"));
        QTimer::singleShot(0, this, &DeviceWorker::pollDevice);
    } else {
        setState(State::Connected, QStringLiteral("设备已重新连接"));
    }
}

void DeviceWorker::scheduleReconnect()
{
    if (!m_connectionRequested || !m_reconnectTimer) {
        return;
    }
    const int delaySeconds = qMin(15, 1 << qMin(m_reconnectAttempt, 4));
    ++m_reconnectAttempt;
    setState(State::Reconnecting,
             QStringLiteral("等待设备恢复，%1 秒后重试第 %2 次")
                 .arg(delaySeconds)
                 .arg(m_reconnectAttempt));
    m_reconnectTimer->start(delaySeconds * 1000);
}

bool DeviceWorker::emitPendingCameraFrame()
{
    if (m_pendingCameraFrame.isNull()) {
        return false;
    }
    emit frameReady(m_pendingCameraFrame, QDateTime::currentDateTime());
    m_pendingCameraFrame = {};
    return true;
}

void DeviceWorker::shutdown()
{
    m_connectionRequested = false;
    m_monitoringRequested = false;
    m_reconnectAttempt = 0;
    m_consecutiveFailures = 0;
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    closeService();
}
