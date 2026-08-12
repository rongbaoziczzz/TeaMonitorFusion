#include "oceandirectspectrometerservice.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

#include <type_traits>
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

    if (!loadSdk(errorMessage)) {
        return false;
    }

    m_probeDevices();
    const int deviceCount = m_getNumberOfDeviceIds();
    if (deviceCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未检测到海洋光学光谱仪。");
        }
        unloadSdk();
        return false;
    }
    if (deviceCount > 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("检测到 %1 台海洋光学光谱仪，无法确定目标设备，请联系系统管理员。")
                                .arg(deviceCount);
        }
        unloadSdk();
        return false;
    }

    std::vector<long> deviceIds(deviceCount);
    if (m_getDeviceIds(deviceIds.data(), static_cast<unsigned int>(deviceIds.size())) <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学设备列表失败。");
        }
        unloadSdk();
        return false;
    }

    int sdkError = 0;
    m_deviceId = deviceIds.front();
    m_openDevice(m_deviceId, &sdkError);
    if (sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("打开海洋光学光谱仪失败，错误码：%1").arg(sdkError);
        }
        m_deviceId = -1;
        unloadSdk();
        return false;
    }

    char modelBuffer[256] = {0};
    char serialBuffer[256] = {0};
    int modelError = 0;
    int serialError = 0;
    const int modelLength = m_getDeviceType(m_deviceId, &modelError, modelBuffer, sizeof(modelBuffer));
    const int serialLength = m_getSerialNumber(m_deviceId, &serialError, serialBuffer, sizeof(serialBuffer));

    QString manufacturer = QStringLiteral("海洋光学");
    if (m_getManufacturerString) {
        char manufacturerBuffer[256] = {0};
        int manufacturerError = 0;
        const int manufacturerLength = m_getManufacturerString(
            m_deviceId, &manufacturerError, manufacturerBuffer, sizeof(manufacturerBuffer));
        if (manufacturerLength > 0 && manufacturerError == 0) {
            manufacturer = QString::fromLocal8Bit(manufacturerBuffer, manufacturerLength);
        }
    }

    const QString model = modelLength > 0 && modelError == 0
                              ? QString::fromLocal8Bit(modelBuffer, modelLength)
                              : QStringLiteral("未知型号");
    const QString serial = serialLength > 0 && serialError == 0
                               ? QString::fromLocal8Bit(serialBuffer, serialLength)
                               : QStringLiteral("未知序列号");

    m_isOpen = true;
    m_deviceSummary = QStringLiteral("%1 %2，序列号：%3，波段范围 1000-1700 nm。")
                          .arg(manufacturer, model, serial);
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

void OceanDirectSpectrometerService::close()
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (m_closeDevice && m_isOpen && m_deviceId >= 0) {
        int sdkError = 0;
        m_closeDevice(m_deviceId, &sdkError);
    }
    unloadSdk();
#endif
    m_isOpen = false;
    m_deviceId = -1;
    m_deviceSummary = QStringLiteral("海洋光学光谱仪已断开。");
}

bool OceanDirectSpectrometerService::isOpen() const
{
    return m_isOpen;
}

bool OceanDirectSpectrometerService::setIntegrationTimeUs(int integrationTimeUs, QString *errorMessage)
{
    m_integrationTimeUs = integrationTimeUs;
    return applyParameters(errorMessage);
}

bool OceanDirectSpectrometerService::setSmoothing(int smoothing, QString *errorMessage)
{
    m_smoothing = smoothing;
    return applyParameters(errorMessage);
}

bool OceanDirectSpectrometerService::setAverageCount(int averageCount, QString *errorMessage)
{
    m_averageCount = averageCount;
    return applyParameters(errorMessage);
}

bool OceanDirectSpectrometerService::acquire(QVector<double> &wavelengths,
                                             QVector<double> &intensities,
                                             QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (!m_getFormattedSpectrumLength || !m_isOpen || m_deviceId < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("海洋光学光谱仪尚未连接。");
        }
        return false;
    }

    int sdkError = 0;
    const int spectrumLength = m_getFormattedSpectrumLength(m_deviceId, &sdkError);
    if (spectrumLength <= 0 || sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学光谱长度失败，错误码：%1").arg(sdkError);
        }
        return false;
    }

    wavelengths.resize(spectrumLength);
    intensities.resize(spectrumLength);

    const int wavelengthCount = m_getWavelengths(m_deviceId, &sdkError, wavelengths.data(), spectrumLength);
    if (wavelengthCount <= 0 || sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读取海洋光学波长数组失败，错误码：%1").arg(sdkError);
        }
        return false;
    }

    const int intensityCount = m_getFormattedSpectrum(m_deviceId, &sdkError, intensities.data(), spectrumLength);
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
        *errorMessage = QStringLiteral("光谱仪支持组件不可用，请联系系统管理员。");
    }
    return false;
#endif
}

bool OceanDirectSpectrometerService::applyParameters(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (!m_setIntegrationTime || !m_isOpen || m_deviceId < 0) {
        return true;
    }

    int sdkError = 0;
    m_setIntegrationTime(m_deviceId, &sdkError, static_cast<unsigned long>(m_integrationTimeUs));
    if (sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置积分时间失败，错误码：%1").arg(sdkError);
        }
        return false;
    }
    m_setBoxcarWidth(m_deviceId, &sdkError, static_cast<unsigned short>(m_smoothing));
    if (sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置平滑宽度失败，错误码：%1").arg(sdkError);
        }
        return false;
    }
    m_setScansToAverage(m_deviceId, &sdkError, static_cast<unsigned int>(m_averageCount));
    if (sdkError != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置平均次数失败，错误码：%1").arg(sdkError);
        }
        return false;
    }
#endif
    Q_UNUSED(errorMessage);
    return true;
}

bool OceanDirectSpectrometerService::loadSdk(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (m_library.isLoaded()) {
        return true;
    }

    const QString dllPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("OceanDirect.dll"));
    if (!QFileInfo::exists(dllPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("光谱仪支持组件缺失，请联系系统管理员。");
        }
        return false;
    }

    m_library.setFileName(dllPath);
    if (!m_library.load()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("光谱仪支持组件加载失败，请联系系统管理员。");
        }
        return false;
    }

    auto resolve = [this, errorMessage](auto &function, const char *symbol) {
        using FunctionType = std::decay_t<decltype(function)>;
        function = reinterpret_cast<FunctionType>(m_library.resolve(symbol));
        if (!function && errorMessage) {
            *errorMessage = QStringLiteral("光谱仪支持组件不完整，请联系系统管理员。");
        }
        return function != nullptr;
    };

    const bool complete = resolve(m_probeDevices, "odapi_probe_devices") &&
                          resolve(m_getNumberOfDeviceIds, "odapi_get_number_of_device_ids") &&
                          resolve(m_getDeviceIds, "odapi_get_device_ids") &&
                          resolve(m_openDevice, "odapi_open_device") &&
                          resolve(m_closeDevice, "odapi_close_device") &&
                          resolve(m_getDeviceType, "odapi_get_device_type") &&
                          resolve(m_getSerialNumber, "odapi_get_serial_number") &&
                          resolve(m_getFormattedSpectrumLength, "odapi_get_formatted_spectrum_length") &&
                          resolve(m_getWavelengths, "odapi_get_wavelengths") &&
                          resolve(m_getFormattedSpectrum, "odapi_get_formatted_spectrum") &&
                          resolve(m_setIntegrationTime, "odapi_set_integration_time_micros") &&
                          resolve(m_setBoxcarWidth, "odapi_set_boxcar_width") &&
                          resolve(m_setScansToAverage, "odapi_set_scans_to_average") &&
                          resolve(m_shutdown, "odapi_shutdown");

    m_getManufacturerString = reinterpret_cast<GetTextFn>(
        m_library.resolve("odapi_adv_get_device_manufacturer_string"));

    if (!complete) {
        unloadSdk();
        return false;
    }
    return true;
#else
    Q_UNUSED(errorMessage);
    return false;
#endif
}

void OceanDirectSpectrometerService::unloadSdk()
{
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
    if (m_shutdown && m_library.isLoaded()) {
        m_shutdown();
    }
    if (m_library.isLoaded()) {
        m_library.unload();
    }

    m_probeDevices = nullptr;
    m_getNumberOfDeviceIds = nullptr;
    m_getDeviceIds = nullptr;
    m_openDevice = nullptr;
    m_closeDevice = nullptr;
    m_getDeviceType = nullptr;
    m_getSerialNumber = nullptr;
    m_getManufacturerString = nullptr;
    m_getFormattedSpectrumLength = nullptr;
    m_getWavelengths = nullptr;
    m_getFormattedSpectrum = nullptr;
    m_setIntegrationTime = nullptr;
    m_setBoxcarWidth = nullptr;
    m_setScansToAverage = nullptr;
    m_shutdown = nullptr;
#endif
}
