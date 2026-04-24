#pragma once

#include "abstractcameraservice.h"

class EBusCameraService : public AbstractCameraService
{
public:
    EBusCameraService();
    ~EBusCameraService() override;

    QString serviceName() const override;
    QString sdkName() const override;
    QString deviceSummary() const override;
    bool open(QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    void setExposureMs(int exposureMs) override;
    void setGain(int gain) override;
    QImage grabFrame() override;

private:
    bool m_isOpen = false;
    int m_exposureMs = 12;
    int m_gain = 1;
    QString m_deviceSummary = QStringLiteral("尚未连接 eBUS 工业相机。");

#ifdef TEA_MONITOR_HAS_EBUS_SDK
    void *m_device = nullptr;
    const void *m_selectedDeviceInfo = nullptr;
    QString m_modelName;
#endif
};
