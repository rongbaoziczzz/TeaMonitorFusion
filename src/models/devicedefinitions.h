#pragma once

#include <QString>
#include <QVector>

struct DeviceProfile
{
    DeviceProfile() = default;

    DeviceProfile(const QString &profileId,
                  const QString &profileName,
                  const QString &profileDescription,
                  const QString &profileWavelengthRange,
                  const QString &profileSdkName,
                  bool profileIsMock,
                  bool profileIsAvailable,
                  const QString &profileConnectionId = QString(),
                  const QString &profileNetworkAddress = QString(),
                  const QString &profileInterfaceAddress = QString())
        : id(profileId)
        , name(profileName)
        , description(profileDescription)
        , wavelengthRange(profileWavelengthRange)
        , sdkName(profileSdkName)
        , isMock(profileIsMock)
        , isAvailable(profileIsAvailable)
        , connectionId(profileConnectionId)
        , networkAddress(profileNetworkAddress)
        , interfaceAddress(profileInterfaceAddress)
    {
    }

    QString id;
    QString name;
    QString description;
    QString wavelengthRange;
    QString sdkName;
    bool isMock = false;
    bool isAvailable = true;
    QString connectionId;
    QString networkAddress;
    QString interfaceAddress;
};

inline QVector<DeviceProfile> availableSpectrometers()
{
    return {
        {"none", "本次不启用光谱仪", "仅执行工业相机检测。", "-", "-", true, true},
#ifdef TEA_MONITOR_HAS_OCEANDIRECT
        {"ocean-direct", "海洋光学近红外光谱仪", "1000-1700 nm 近红外光谱检测。", "1000-1700 nm", "OceanDirect SDK", false, true},
#else
        {"ocean-direct", "海洋光学近红外光谱仪", "设备支持组件未安装。", "1000-1700 nm", "OceanDirect SDK", false, false},
#endif
#ifdef TEA_MONITOR_HAS_SEASDK
        {"oceanhood", "如海广电 XS11639 光谱仪", "2048 点光谱采集。", "约 344-1070 nm", "SeaSDK", false, true},
#else
        {"oceanhood", "如海广电 XS11639 光谱仪", "设备支持组件未安装。", "约 344-1070 nm", "SeaSDK", false, false},
#endif
        {"sim-spec", "离线光谱数据源", "提供标准光谱数据。", "420-804 nm", "内置数据源", true, true}
    };
}

inline QVector<DeviceProfile> availableCameras()
{
    return {
        {"none", "本次不启用工业相机", "仅执行光谱检测。", "-", "-", true, true},
#ifdef TEA_MONITOR_HAS_ARAVIS
        {"aravis-camera", "IMPERX B1621C 工业相机", "GigE Vision 工业视觉设备，按序列号 56S014 自动发现当前地址。", "-", "Aravis 0.8", false, true, "56S014", "169.254.4.4", "169.254.53.32"},
#else
        {"aravis-camera", "IMPERX B1621C 工业相机", "设备支持组件未安装。", "-", "Aravis 0.8", false, false, "56S014", "169.254.4.4", "169.254.53.32"},
#endif
        {"sim-camera", "离线图像数据源", "提供连续图像数据。", "-", "内置数据源", true, true}
    };
}
