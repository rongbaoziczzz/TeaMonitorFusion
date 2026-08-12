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
    virtual bool setExposureMs(int exposureMs, QString *errorMessage = nullptr) = 0;
    virtual bool setGain(int gain, QString *errorMessage = nullptr) = 0;
    virtual bool setParameters(int exposureMs, int gain, QString *errorMessage = nullptr)
    {
        return setExposureMs(exposureMs, errorMessage) && setGain(gain, errorMessage);
    }
    virtual bool recoverStream(QString *errorMessage = nullptr)
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前相机服务不支持独立恢复图像流。");
        }
        return false;
    }
    virtual QImage grabFrame(QString *errorMessage = nullptr) = 0;
};
