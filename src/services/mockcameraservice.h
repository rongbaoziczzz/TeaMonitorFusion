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
    bool setExposureMs(int exposureMs, QString *errorMessage = nullptr) override;
    bool setGain(int gain, QString *errorMessage = nullptr) override;
    QImage grabFrame(QString *errorMessage = nullptr) override;

private:
    bool m_isOpen = false;
    int m_exposureMs = 12;
    int m_gain = 1;
    int m_frameIndex = 0;
};
