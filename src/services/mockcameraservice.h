#pragma once

#include "abstractcameraservice.h"

class MockCameraService : public AbstractCameraService
{
public:
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
    int m_frameIndex = 0;
};
