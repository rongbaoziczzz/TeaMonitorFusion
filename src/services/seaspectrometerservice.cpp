#include "seaspectrometerservice.h"

#ifdef TEA_MONITOR_HAS_SEASDK
#include "DllDecl.h"
#include "SeaDef.h"
#include "SeaSDKWrapper.h"
#endif

SeaSpectrometerService::SeaSpectrometerService() = default;

SeaSpectrometerService::~SeaSpectrometerService()
{
    close();
}

QString SeaSpectrometerService::serviceName() const
{
    return QStringLiteral("如海广电光谱仪");
}

QString SeaSpectrometerService::sdkName() const
{
    return QStringLiteral("SeaSDK");
}

QString SeaSpectrometerService::deviceSummary() const
{
    return m_deviceSummary;
}

bool SeaSpectrometerService::open(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_SEASDK
    if (m_isOpen) {
        return true;
    }

    int errorCode = 0;
    const int deviceCount = seasdk_open_all_spectrometers(&errorCode);
    if (deviceCount <= 0 || errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("SeaSDK 打开如海广电光谱仪失败，错误码：%1").arg(errorCode);
        }
        m_isOpen = false;
        return false;
    }

    m_spectrometerIndex = 0;
    m_isOpen = true;
    m_deviceSummary = QStringLiteral("已连接如海广电光谱仪，波段范围 400-1100 nm。");
    applyParameters();
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("当前构建未链接 SeaSDK，无法连接如海广电光谱仪。");
    }
    return false;
#endif
}

void SeaSpectrometerService::close()
{
#ifdef TEA_MONITOR_HAS_SEASDK
    if (!m_isOpen) {
        return;
    }

    int errorCode = 0;
    seasdk_close_all_spectrometers(&errorCode);
    Q_UNUSED(errorCode);
#endif
    m_isOpen = false;
    m_deviceSummary = QStringLiteral("如海广电光谱仪已断开。");
}

bool SeaSpectrometerService::isOpen() const
{
    return m_isOpen;
}

void SeaSpectrometerService::setIntegrationTimeUs(int integrationTimeUs)
{
    m_integrationTimeUs = integrationTimeUs;
    applyParameters();
}

void SeaSpectrometerService::setSmoothing(int smoothing)
{
    m_smoothing = smoothing;
    applyParameters();
}

void SeaSpectrometerService::setAverageCount(int averageCount)
{
    m_averageCount = averageCount;
    applyParameters();
}

bool SeaSpectrometerService::acquire(QVector<double> &wavelengths,
                                     QVector<double> &intensities,
                                     QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_SEASDK
    if (!m_isOpen) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("如海广电光谱仪尚未连接。");
        }
        return false;
    }

    int errorCode = 0;
    const int pixelCount = seasdk_get_formatted_spectrum_length(m_spectrometerIndex, &errorCode);
    if (pixelCount <= 0 || errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取光谱长度失败，错误码：%1").arg(errorCode);
        }
        return false;
    }

    wavelengths.resize(pixelCount);
    intensities.resize(pixelCount);

    const int wavelengthCount = seasdk_get_wavelengths(m_spectrometerIndex, &errorCode, wavelengths.data(), pixelCount);
    if (wavelengthCount <= 0 || errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取波长数组失败，错误码：%1").arg(errorCode);
        }
        return false;
    }

    const int intensityCount = seasdk_get_formatted_spectrum(m_spectrometerIndex, &errorCode, intensities.data(), pixelCount);
    if (intensityCount <= 0 || errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取光谱数据失败，错误码：%1").arg(errorCode);
        }
        return false;
    }

    return true;
#else
    Q_UNUSED(wavelengths);
    Q_UNUSED(intensities);
    if (errorMessage) {
        *errorMessage = QStringLiteral("当前构建未包含 SeaSDK。");
    }
    return false;
#endif
}

void SeaSpectrometerService::applyParameters()
{
#ifdef TEA_MONITOR_HAS_SEASDK
    if (!m_isOpen) {
        return;
    }

    int errorCode = 0;
    seasdk_set_integration_time_microsec(m_spectrometerIndex, &errorCode, m_integrationTimeUs);
    seasdk_set_boxcar(m_spectrometerIndex, &errorCode, m_smoothing);
    seasdk_set_average(m_spectrometerIndex, &errorCode, m_averageCount);
#endif
}
