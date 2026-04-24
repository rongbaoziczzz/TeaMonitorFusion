#pragma once

#include "abstractspectrometerservice.h"

class SeaSpectrometerService : public AbstractSpectrometerService
{
public:
    SeaSpectrometerService();
    ~SeaSpectrometerService() override;

    QString serviceName() const override;
    QString sdkName() const override;
    QString deviceSummary() const override;
    bool open(QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    void setIntegrationTimeUs(int integrationTimeUs) override;
    void setSmoothing(int smoothing) override;
    void setAverageCount(int averageCount) override;
    bool acquire(QVector<double> &wavelengths,
                 QVector<double> &intensities,
                 QString *errorMessage = nullptr) override;

private:
    bool m_isOpen = false;
    int m_spectrometerIndex = 0;
    int m_integrationTimeUs = 100000;
    int m_smoothing = 5;
    int m_averageCount = 5;
    QString m_deviceSummary = QStringLiteral("尚未连接如海广电光谱仪。");

    void applyParameters();
};
