#pragma once

#include <QString>
#include <QVector>

struct DeviceProfile
{
    QString id;
    QString name;
    QString description;
    QString wavelengthRange;
    QString sdkName;
    bool isMock = false;
};

inline QVector<DeviceProfile> availableSpectrometers()
{
    return {
        {"none", "本次不启用光谱仪", "仅使用工业相机流程。", "-", "-", true},
        {"ocean-direct", "海洋光学光谱仪", "使用 OceanDirect SDK，适配 1000-1700 nm 近红外检测。", "1000-1700 nm", "OceanDirect SDK", false},
        {"oceanhood", "如海广电光谱仪", "使用 SeaSDK，适配 400-1100 nm 检测。", "400-1100 nm", "SeaSDK", false},
        {"sim-spec", "光谱模拟器", "用于演示、培训和无设备调试。", "420-804 nm", "内置模拟数据", true}
    };
}

inline QVector<DeviceProfile> availableCameras()
{
    return {
        {"none", "本次不启用工业相机", "仅使用光谱检测流程。", "-", "-", true},
        {"ebus-camera", "eBUS 工业相机", "优先使用本地 Pleora eBUS 开发 SDK 接入工业相机。", "-", "Pleora eBUS SDK", false},
        {"sim-camera", "工业相机模拟器", "生成实时模拟画面，便于培训和流程联调。", "-", "内置模拟数据", true}
    };
}
