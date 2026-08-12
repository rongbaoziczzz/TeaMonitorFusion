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
    if (deviceCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("打开如海广电光谱仪失败，错误码：%1").arg(errorCode);
        }
        m_isOpen = false;
        return false;
    }
    if (deviceCount > 1) {
        int closeError = 0;
        seasdk_close_all_spectrometers(&closeError);
        if (errorMessage) {
            *errorMessage = QStringLiteral("检测到 %1 台如海广电光谱仪，无法确定目标设备，请联系系统管理员。")
                                .arg(deviceCount);
        }
        m_isOpen = false;
        return false;
    }

    m_spectrometerIndex = 0;
    m_isOpen = true;

    char modelBuffer[128] = {};
    char serialBuffer[128] = {};
    int modelError = 0;
    int serialError = 0;
    seasdk_get_model(m_spectrometerIndex, &modelError, modelBuffer, sizeof(modelBuffer));
    seasdk_get_serial_number(m_spectrometerIndex, &serialError, serialBuffer, sizeof(serialBuffer));

    const QString model = modelError == 0 ? QString::fromLocal8Bit(modelBuffer).trimmed() : QString();
    const QString serial = serialError == 0 ? QString::fromLocal8Bit(serialBuffer).trimmed() : QString();
    QString wavelengthRange = QStringLiteral("设备标定范围");
    int wavelengthError = 0;
    const int wavelengthCount = seasdk_get_formatted_spectrum_length(m_spectrometerIndex, &wavelengthError);
    if (wavelengthCount > 1 && wavelengthError == 0) {
        QVector<double> wavelengthValues(wavelengthCount);
        wavelengthError = 0;
        const int returnedCount = seasdk_get_wavelengths(m_spectrometerIndex,
                                                         &wavelengthError,
                                                         wavelengthValues.data(),
                                                         wavelengthCount);
        if (returnedCount > 1 && wavelengthError == 0) {
            wavelengthRange = QStringLiteral("%1-%2 nm")
                                  .arg(wavelengthValues.first(), 0, 'f', 1)
                                  .arg(wavelengthValues.last(), 0, 'f', 1);
        }
    }
    if (!model.isEmpty() || !serial.isEmpty()) {
        m_deviceSummary = QStringLiteral("已连接如海广电光谱仪：%1，序列号：%2，波段范围 %3。")
                              .arg(model.isEmpty() ? QStringLiteral("未知型号") : model,
                                   serial.isEmpty() ? QStringLiteral("未知") : serial,
                                   wavelengthRange);
    } else {
        m_deviceSummary = QStringLiteral("已连接如海广电光谱仪，波段范围 %1。")
                              .arg(wavelengthRange);
    }
    if (!applyParameters(errorMessage)) {
        close();
        return false;
    }
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("光谱仪支持组件不可用，请联系系统管理员。");
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

bool SeaSpectrometerService::setIntegrationTimeUs(int integrationTimeUs, QString *errorMessage)
{
    m_integrationTimeUs = integrationTimeUs;
    return applyParameters(errorMessage);
}

bool SeaSpectrometerService::setSmoothing(int smoothing, QString *errorMessage)
{
    m_smoothing = smoothing;
    return applyParameters(errorMessage);
}

bool SeaSpectrometerService::setAverageCount(int averageCount, QString *errorMessage)
{
    m_averageCount = averageCount;
    return applyParameters(errorMessage);
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

    intensities.fill(0.0);
    const int scanCount = m_useSoftwareAverage ? qMax(1, m_averageCount) : 1;
    QVector<double> scan(pixelCount);
    for (int scanIndex = 0; scanIndex < scanCount; ++scanIndex) {
        errorCode = 0;
        const int intensityCount = seasdk_get_formatted_spectrum(
            m_spectrometerIndex, &errorCode, scan.data(), pixelCount);
        if (intensityCount <= 0 || errorCode != 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("读取第 %1/%2 次光谱数据失败，错误码：%3")
                                    .arg(scanIndex + 1)
                                    .arg(scanCount)
                                    .arg(errorCode);
            }
            return false;
        }
        for (int pixel = 0; pixel < pixelCount; ++pixel) {
            intensities[pixel] += scan.at(pixel);
        }
    }
    if (scanCount > 1) {
        for (double &intensity : intensities) {
            intensity /= scanCount;
        }
    }

    return true;
#else
    Q_UNUSED(wavelengths);
    Q_UNUSED(intensities);
    if (errorMessage) {
        *errorMessage = QStringLiteral("光谱仪支持组件不可用，请联系系统管理员。");
    }
    return false;
#endif
}

bool SeaSpectrometerService::applyParameters(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_SEASDK
    if (!m_isOpen) {
        return true;
    }

    int errorCode = 0;
    seasdk_set_integration_time_microsec(m_spectrometerIndex, &errorCode, m_integrationTimeUs);
    if (errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置积分时间失败，错误码：%1").arg(errorCode);
        }
        return false;
    }
    seasdk_set_boxcar(m_spectrometerIndex, &errorCode, m_smoothing);
    if (errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置平滑宽度失败，错误码：%1").arg(errorCode);
        }
        return false;
    }
    seasdk_set_average(m_spectrometerIndex, &errorCode, m_averageCount);
    if (errorCode != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置平均次数失败，错误码：%1").arg(errorCode);
        }
        return false;
    }

    int readbackError = 0;
    const int appliedAverage = seasdk_get_average(m_spectrometerIndex, &readbackError);
    m_useSoftwareAverage = readbackError == 0 && appliedAverage != m_averageCount;
    if (m_useSoftwareAverage) {
        int resetError = 0;
        seasdk_set_average(m_spectrometerIndex, &resetError, 1);
        if (resetError != 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("设备不支持请求的硬件平均次数，且切换软件平均失败，错误码：%1")
                                    .arg(resetError);
            }
            return false;
        }
    }
#endif
    Q_UNUSED(errorMessage);
    return true;
}
