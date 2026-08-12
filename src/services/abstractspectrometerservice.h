#pragma once

#include <QString>
#include <QVector>

class AbstractSpectrometerService
{
public:
    virtual ~AbstractSpectrometerService() = default;

    virtual QString serviceName() const = 0;
    virtual QString sdkName() const = 0;
    virtual QString deviceSummary() const = 0;
    virtual bool open(QString *errorMessage = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool setIntegrationTimeUs(int integrationTimeUs, QString *errorMessage = nullptr) = 0;
    virtual bool setSmoothing(int smoothing, QString *errorMessage = nullptr) = 0;
    virtual bool setAverageCount(int averageCount, QString *errorMessage = nullptr) = 0;
    virtual bool acquire(QVector<double> &wavelengths,
                         QVector<double> &intensities,
                         QString *errorMessage = nullptr) = 0;
};
