#include "models/spectrumcalibration.h"
#include "services/mockcameraservice.h"
#include "services/cameraframeconverter.h"
#include "services/mockspectrometerservice.h"
#include "services/deviceworker.h"
#include "services/auditstore.h"

#include <QDebug>
#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>
#include <QTemporaryDir>
#include <QtMath>

#include <cstdio>

namespace {
int failures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        qCritical() << "FAILED:" << message;
        std::fprintf(stderr, "FAILED: %s\n", message);
        ++failures;
    }
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    const QVector<double> wavelengths{500.0, 501.0, 502.0};
    SpectrumReference dark{wavelengths, {10.0, 20.0, 30.0}, QDateTime::currentDateTime(), "sim-spec", 100000, 5, 3};
    SpectrumReference white{wavelengths, {110.0, 220.0, 330.0}, QDateTime::currentDateTime(), "sim-spec", 100000, 5, 3};
    QString error;
    check(SpectrumCalibration::referencesMatch(dark, white, wavelengths, "sim-spec", 100000, 5, 3, &error),
          "matching references should be accepted");
    check(!SpectrumCalibration::referencesMatch(dark, white, wavelengths, "sim-spec", 200000, 5, 3, &error),
          "parameter mismatch should invalidate references");

    QVector<double> reflectance;
    check(SpectrumCalibration::calculateReflectance({60.0, 120.0, 180.0}, dark, white, reflectance, &error),
          "reflectance calculation should succeed");
    check(reflectance.size() == 3 && qAbs(reflectance.at(0) - 0.5) < 1e-12 &&
              qAbs(reflectance.at(1) - 0.5) < 1e-12 && qAbs(reflectance.at(2) - 0.5) < 1e-12,
          "reflectance values should be correct");

    QVector<double> transmittance;
    check(SpectrumCalibration::calculateTransmittance({35.0, 70.0, 105.0}, dark, white, transmittance, &error),
          "transmittance calculation should succeed");
    check(transmittance.size() == 3 && qAbs(transmittance.at(0) - 0.25) < 1e-12 &&
              qAbs(transmittance.at(1) - 0.25) < 1e-12 && qAbs(transmittance.at(2) - 0.25) < 1e-12,
          "transmittance values should be correct");

    SpectrumReference invalidWhite = white;
    invalidWhite.intensities[1] = dark.intensities[1];
    check(!SpectrumCalibration::calculateReflectance({60.0, 120.0, 180.0}, dark, invalidWhite, reflectance, &error),
          "zero reference range should be rejected");
    check(!SpectrumCalibration::calculateTransmittance({60.0, 120.0, 180.0}, dark, invalidWhite,
                                                       transmittance, &error),
          "invalid incident reference should be rejected");

    MockSpectrometerService spectrometer;
    check(spectrometer.open(&error), "mock spectrometer should open");
    check(spectrometer.setIntegrationTimeUs(120000, &error), "mock integration time should apply");
    QVector<double> mockWavelengths;
    QVector<double> mockIntensities;
    check(spectrometer.acquire(mockWavelengths, mockIntensities, &error), "mock spectrum acquisition should succeed");
    check(!mockWavelengths.isEmpty() && mockWavelengths.size() == mockIntensities.size(),
          "mock spectrum arrays should be valid");

    MockCameraService camera;
    check(camera.open(&error), "mock camera should open");
    check(camera.setExposureMs(15, &error) && camera.setGain(2, &error), "mock camera parameters should apply");
    check(!camera.grabFrame(&error).isNull(), "mock camera should return an image");

    const uchar monoPixels[] = {1, 2, 3, 4};
    const QImage mono = CameraFrameConverter::fromPfnc(monoPixels,
                                                        sizeof(monoPixels),
                                                        2,
                                                        2,
                                                        2,
                                                        CameraFrameConverter::Mono8);
    check(!mono.isNull() && mono.format() == QImage::Format_Grayscale8 &&
              mono.pixelColor(1, 1).value() == 4,
          "Mono8 frames should convert to grayscale images");

    const uchar rgbPixels[] = {255, 0, 0, 0, 255, 0};
    const QImage rgb = CameraFrameConverter::fromPfnc(rgbPixels,
                                                       sizeof(rgbPixels),
                                                       2,
                                                       1,
                                                       6,
                                                       CameraFrameConverter::Rgb8Packed);
    check(!rgb.isNull() && rgb.pixelColor(0, 0).red() == 255 &&
              rgb.pixelColor(1, 0).green() == 255,
          "RGB8 packed frames should preserve channel order");

    const uchar mono12Pixels[] = {0xff, 0x0f, 0x00, 0x00};
    const QImage mono12 = CameraFrameConverter::fromPfnc(mono12Pixels,
                                                          sizeof(mono12Pixels),
                                                          2,
                                                          1,
                                                          4,
                                                          CameraFrameConverter::Mono12);
    check(!mono12.isNull() && mono12.pixelColor(0, 0).value() == 255 &&
              mono12.pixelColor(1, 0).value() == 0,
          "unpacked Mono12 frames should be scaled to 8-bit grayscale");

    const uchar bayerPixels[] = {255, 128, 64, 32};
    const QImage bayer = CameraFrameConverter::fromPfnc(bayerPixels,
                                                         sizeof(bayerPixels),
                                                         2,
                                                         2,
                                                         2,
                                                         CameraFrameConverter::BayerRG8);
    check(!bayer.isNull() && bayer.format() == QImage::Format_RGB888,
          "Bayer8 frames should convert to RGB images");

    const auto cameraProfiles = availableCameras();
    bool imperxProfileFound = false;
    for (const auto &profile : cameraProfiles) {
        if (profile.id == QStringLiteral("aravis-camera")) {
            imperxProfileFound = true;
            check(profile.connectionId == QStringLiteral("56S014"),
                  "the IMPERX profile should bind the photographed camera serial number");
            check(profile.networkAddress == QStringLiteral("169.254.4.4"),
                  "the IMPERX profile should provide the camera fixed IPv4 fallback");
            check(profile.interfaceAddress == QStringLiteral("169.254.53.32"),
                  "the IMPERX profile should bind the dedicated host interface");
        }
    }
    check(imperxProfileFound, "the Aravis camera profile should be registered");

    DeviceProfile noDevice;
    const DeviceProfile simulatedSpectrum{"sim-spec", "sim", "", "", "", true, true};
    DeviceWorker spectrumWorker(simulatedSpectrum, noDevice);
    bool spectrumWorkerConnected = false;
    bool spectrumWorkerProducedData = false;
    QObject::connect(&spectrumWorker, &DeviceWorker::stateChanged,
                     [&spectrumWorkerConnected](DeviceWorker::State state, const QString &) {
                         if (state == DeviceWorker::State::Connected) {
                             spectrumWorkerConnected = true;
                         }
                     });
    QEventLoop spectrumLoop;
    QObject::connect(&spectrumWorker, &DeviceWorker::spectrumReady,
                     [&spectrumWorkerProducedData, &spectrumLoop](const QVector<double> &, const QVector<double> &, const QDateTime &) {
                         spectrumWorkerProducedData = true;
                         spectrumLoop.quit();
                     });
    spectrumWorker.initialize();
    spectrumWorker.connectDevice();
    spectrumWorker.startMonitoring();
    QTimer::singleShot(2000, &spectrumLoop, &QEventLoop::quit);
    spectrumLoop.exec();
    check(spectrumWorkerConnected, "device worker should reach connected state");
    check(spectrumWorkerProducedData, "device worker should emit spectrum data");
    spectrumWorker.shutdown();

    const DeviceProfile simulatedCamera{"sim-camera", "sim", "", "", "", true, true};
    DeviceWorker cameraWorker(noDevice, simulatedCamera);
    bool cameraWorkerProducedFrame = false;
    QEventLoop cameraLoop;
    QObject::connect(&cameraWorker, &DeviceWorker::frameReady,
                     [&cameraWorkerProducedFrame, &cameraLoop](const QImage &, const QDateTime &) {
                         cameraWorkerProducedFrame = true;
                         cameraLoop.quit();
                     });
    cameraWorker.initialize();
    cameraWorker.connectDevice();
    cameraWorker.startMonitoring();
    QTimer::singleShot(2000, &cameraLoop, &QEventLoop::quit);
    cameraLoop.exec();
    check(cameraWorkerProducedFrame, "device worker should emit a camera frame");
    cameraWorker.shutdown();

    QTemporaryDir auditDirectory;
    const QString auditPath = auditDirectory.filePath(QStringLiteral("audit.sqlite"));
    const bool auditInitialized = AuditStore::initialize(auditPath, &error);
    if (!auditInitialized) {
        std::fprintf(stderr, "AUDIT INIT ERROR: %s (temp valid=%d, path=%s)\n",
                     error.toUtf8().constData(), auditDirectory.isValid() ? 1 : 0, auditPath.toUtf8().constData());
    }
    check(auditInitialized, "audit database should initialize");
    const bool auditRecorded = AuditStore::recordExport("sample-1", "batch-1", "operator-1", "sim-spec", "sim",
                                   "spectrum_csv", QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                   auditDirectory.filePath(QStringLiteral("sample.csv")),
                                   QJsonObject{{QStringLiteral("integration_time_us"), 100000}}, true, &error);
    if (!auditRecorded) {
        std::fprintf(stderr, "AUDIT RECORD ERROR: %s\n", error.toUtf8().constData());
    }
    check(auditRecorded, "audit record should be inserted");
    check(AuditStore::databasePath() == auditPath, "audit database path should be reported");
    check(AuditStore::recentExports(10, &error).size() == 1, "audit record should be queryable");
    AuditStore::shutdown();

    if (failures == 0) {
        qInfo() << "All core tests passed.";
    }
    return failures == 0 ? 0 : 1;
}
