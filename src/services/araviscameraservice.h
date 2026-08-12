#pragma once

#include "abstractcameraservice.h"

class AravisCameraService : public AbstractCameraService
{
public:
    explicit AravisCameraService(const QString &expectedSerialNumber = QString(),
                                 const QString &preferredAddress = QString(),
                                 const QString &preferredInterfaceAddress = QString());
    ~AravisCameraService() override;

    QString serviceName() const override;
    QString sdkName() const override;
    QString deviceSummary() const override;
    bool open(QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    bool setExposureMs(int exposureMs, QString *errorMessage = nullptr) override;
    bool setGain(int gain, QString *errorMessage = nullptr) override;
    bool setParameters(int exposureMs, int gain, QString *errorMessage = nullptr) override;
    bool recoverStream(QString *errorMessage = nullptr) override;
    QImage grabFrame(QString *errorMessage = nullptr) override;

private:
    QString m_expectedSerialNumber;
    QString m_preferredAddress;
    QString m_preferredInterfaceAddress;
    QString m_deviceSummary = QStringLiteral("IMPERX GigE Vision 相机尚未连接。");
    void *m_camera = nullptr;
    void *m_stream = nullptr;
    bool m_isOpen = false;
    int m_exposureMs = 12;
    int m_gain = 1;

    bool applyCameraParameters(QString *errorMessage = nullptr);
    bool startStream(QString *errorMessage = nullptr);
    void stopStream();
};
