#include "oceandirectspectrometerservice.h"

#ifdef TEA_MONITOR_HAS_OCEANDIRECT
#include "api/OceanDirectAPI.h"
#include "api/advanced/Advance.h"
#include "api/advanced/DeviceInformationAPI.h"
#endif

#include <vector>

OceanDirectSpectrometerService::OceanDirectSpectrometerService() = default;

OceanDirectSpectrometerService::~OceanDirectSpectrometerService()
{
    close();
}

QString OceanDirectSpectrometerService::serviceName() const
{
    return QStringLiteral("海洋光学光谱仪");
}

QString OceanDirectSpectrometerService::sdkName() const
{
    return QStringLiteral("OceanDirect SDK");
}

QString OceanDirectSpectrometerService::deviceSummary() const
{
    return m_deviceSummary;
}

bool OceanDirectSpectrometerService::open(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (m_isOpen) {
        return true;
    }

    m_api = oceandirect::api::OceanDirectAPI::getInstance();
    if (!m_api) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OceanDirect SDK 初始化失败。");
        }
        return false;
    }

    m_api->probeDevices();
    const int deviceCount = m_api->getNumberOfDeviceIDs();
    if (deviceCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未检测到海洋光学光谱仪。");
        }
        return false;
    }

    std::vector<long> deviceIds(deviceCount);
    if (m_api->getDeviceIDs(deviceIds.data(), static_cast<unsigned long>(deviceIds.size())) <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学设备列表失败。");
        }
        return false;
    }

    int sdkError = 0;
    m_deviceId = deviceIds.front();
    m_api->openDevice(m_deviceId, &sdkError);
    if (sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("OceanDirect 打开设备失败，错误码：%1").arg(sdkError);
        }
        m_deviceId = -1;
        return false;
    }

    char modelBuffer[256] = {0};
    char serialBuffer[256] = {0};
    m_api->getDeviceModelText(m_deviceId, &sdkError, modelBuffer, sizeof(modelBuffer));
    m_api->getSerialNumber(m_deviceId, &sdkError, serialBuffer, sizeof(serialBuffer));

    QString manufacturer = QStringLiteral("海洋光学");
    auto *advanced = m_api->AdvancedControl();
    if (advanced) {
        if (auto *info = advanced->DeviceInformationControl()) {
            char manufacturerBuffer[256] = {0};
            const int manufacturerLength = info->getManufacturerString(m_deviceId, &sdkError, manufacturerBuffer, sizeof(manufacturerBuffer));
            if (manufacturerLength > 0 && sdkError == 0) {
                manufacturer = QString::fromLocal8Bit(manufacturerBuffer, manufacturerLength);
            }
        }
    }

    m_isOpen = true;
    m_deviceSummary = QStringLiteral("%1 %2，序列号：%3，波段范围 1000-1700 nm。")
                          .arg(manufacturer,
                               QString::fromLocal8Bit(modelBuffer),
                               QString::fromLocal8Bit(serialBuffer));
    applyParameters();
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("当前构建未启用 OceanDirect SDK。已检测到本机安装包，但当前 MinGW 构建通常无法直接链接官方库，建议后续切换到 MSVC Qt 套件。");
    }
    return false;
#endif
}

void OceanDirectSpectrometerService::close()
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (m_api && m_isOpen && m_deviceId >= 0) {
        int sdkError = 0;
        m_api->closeDevice(m_deviceId, &sdkError);
    }
#endif
    m_isOpen = false;
    m_deviceId = -1;
    m_deviceSummary = QStringLiteral("海洋光学光谱仪已断开。");
}

bool OceanDirectSpectrometerService::isOpen() const
{
    return m_isOpen;
}

void OceanDirectSpectrometerService::setIntegrationTimeUs(int integrationTimeUs)
{
    m_integrationTimeUs = integrationTimeUs;
    applyParameters();
}

void OceanDirectSpectrometerService::setSmoothing(int smoothing)
{
    m_smoothing = smoothing;
    applyParameters();
}

void OceanDirectSpectrometerService::setAverageCount(int averageCount)
{
    m_averageCount = averageCount;
    applyParameters();
}

bool OceanDirectSpectrometerService::acquire(QVector<double> &wavelengths,
                                             QVector<double> &intensities,
                                             QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (!m_api || !m_isOpen || m_deviceId < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("海洋光学光谱仪尚未连接。");
        }
        return false;
    }

    int sdkError = 0;
    const int spectrumLength = m_api->getFormattedSpectrumLength(m_deviceId, &sdkError);
    if (spectrumLength <= 0 || sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学光谱长度失败，错误码：%1").arg(sdkError);
        }
        return false;
    }

    wavelengths.resize(spectrumLength);
    intensities.resize(spectrumLength);

    const int wavelengthCount = m_api->getWavelengths(m_deviceId, &sdkError, wavelengths.data(), spectrumLength);
    if (wavelengthCount <= 0 || sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学波长数组失败，错误码：%1").arg(sdkError);
        }
        return false;
    }

    const int intensityCount = m_api->getFormattedSpectrum(m_deviceId, &sdkError, intensities.data(), spectrumLength);
    if (intensityCount <= 0 || sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学光谱数据失败，错误码：%1").arg(sdkError);
        }
        return false;
    }

    return true;
#else
    Q_UNUSED(wavelengths);
    Q_UNUSED(intensities);
    if (errorMessage) {
        *errorMessage = QStringLiteral("当前构建未启用 OceanDirect SDK。");
    }
    return false;
#endif
}

void OceanDirectSpectrometerService::applyParameters()
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (!m_api || !m_isOpen || m_deviceId < 0) {
        return;
    }

    int sdkError = 0;
    m_api->setIntegrationTimeMicros(m_deviceId, &sdkError, static_cast<unsigned long>(m_integrationTimeUs));
    m_api->setBoxcarWidth(m_deviceId, &sdkError, static_cast<unsigned short>(m_smoothing));
    m_api->setScansToAverage(m_deviceId, &sdkError, static_cast<unsigned int>(m_averageCount));
#endif
}
