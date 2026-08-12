#pragma once

#include "abstractspectrometerservice.h"

#include <QLibrary>

class OceanDirectSpectrometerService : public AbstractSpectrometerService
{
public:
    OceanDirectSpectrometerService();
    ~OceanDirectSpectrometerService() override;

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
    using ProbeDevicesFn = int (*)();
    using GetNumberOfDeviceIdsFn = int (*)();
    using GetDeviceIdsFn = int (*)(long *, unsigned int);
    using OpenDeviceFn = void (*)(long, int *);
    using CloseDeviceFn = void (*)(long, int *);
    using GetDeviceTypeFn = int (*)(long, int *, char *, unsigned int);
    using GetSerialNumberFn = int (*)(long, int *, char *, int);
    using GetTextFn = int (*)(long, int *, char *, int);
    using GetSpectrumLengthFn = int (*)(long, int *);
    using GetSpectrumFn = int (*)(long, int *, double *, int);
    using SetIntegrationTimeFn = void (*)(long, int *, unsigned long);
    using SetBoxcarWidthFn = void (*)(long, int *, unsigned short);
    using SetScansToAverageFn = void (*)(long, int *, unsigned int);
    using ShutdownFn = void (*)();

    QLibrary m_library;
    ProbeDevicesFn m_probeDevices = nullptr;
    GetNumberOfDeviceIdsFn m_getNumberOfDeviceIds = nullptr;
    GetDeviceIdsFn m_getDeviceIds = nullptr;
    OpenDeviceFn m_openDevice = nullptr;
    CloseDeviceFn m_closeDevice = nullptr;
    GetDeviceTypeFn m_getDeviceType = nullptr;
    GetSerialNumberFn m_getSerialNumber = nullptr;
    GetTextFn m_getManufacturerString = nullptr;
    GetSpectrumLengthFn m_getFormattedSpectrumLength = nullptr;
    GetSpectrumFn m_getWavelengths = nullptr;
    GetSpectrumFn m_getFormattedSpectrum = nullptr;
    SetIntegrationTimeFn m_setIntegrationTime = nullptr;
    SetBoxcarWidthFn m_setBoxcarWidth = nullptr;
    SetScansToAverageFn m_setScansToAverage = nullptr;
    ShutdownFn m_shutdown = nullptr;
    long m_deviceId = -1;
    bool m_isOpen = false;
    int m_integrationTimeUs = 100000;
    int m_smoothing = 5;
    int m_averageCount = 5;
    QString m_deviceSummary = QStringLiteral("尚未连接海洋光学光谱仪。");

    bool loadSdk(QString *errorMessage);
    void unloadSdk();
    bool applyParameters(QString *errorMessage = nullptr);
};
