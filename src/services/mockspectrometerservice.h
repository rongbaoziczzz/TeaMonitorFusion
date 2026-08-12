#pragma once

#include "abstractspectrometerservice.h"

class MockSpectrometerService : public AbstractSpectrometerService
{
public:
    QString serviceName() const override;
    QString sdkName() const override;
    QString deviceSummary() const override;
    bool open(QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool setIntegrationTimeUs(int integrationTimeUs, QString *errorMessage = nullptr) override;
    bool setSmoothing(int smoothing, QString *errorMessage = nullptr) override;
    bool setAverageCount(int averageCount, QString *errorMessage = nullptr) override;
    bool acquire(QVector<double> &wavelengths,
                 QVector<double> &intensities,
                 QString *errorMessage = nullptr) override;

private:
    bool m_isOpen = false;
    int m_integrationTimeUs = 100000;
    int m_smoothing = 5;
    int m_averageCount = 3;
    int m_frameIndex = 0;
};
