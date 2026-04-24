#pragma once

#include <QImage>
#include <QString>

class AbstractCameraService
{
public:
    virtual ~AbstractCameraService() = default;

    virtual QString serviceName() const = 0;
    virtual QString sdkName() const = 0;
    virtual QString deviceSummary() const = 0;
    virtual bool open(QString *errorMessage = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual void setExposureMs(int exposureMs) = 0;
    virtual void setGain(int gain) = 0;
    virtual QImage grabFrame() = 0;
};
