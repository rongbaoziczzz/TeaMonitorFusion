#pragma once

#include "models/devicedefinitions.h"
#include "models/spectrumcalibration.h"
#include "services/deviceworker.h"

#include <QDateTime>
#include <QImage>
#include <QMainWindow>
#include <QVector>

class QCloseEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;
class QWidget;
class SpectrumChartWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const DeviceProfile &spectrometerProfile,
                        const DeviceProfile &cameraProfile,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    void startCameraLivePreview();

signals:
    void requestReturnToLauncher();
    void connectDeviceRequested();
    void disconnectDeviceRequested();
    void startMonitoringRequested();
    void stopMonitoringRequested();
    void spectrometerParametersRequested(int integrationTimeUs, int smoothing, int averageCount);
    void cameraParametersRequested(int exposureMs, int gain);
    void shutdownWorkerRequested();

private slots:
    void connectDevices();
    void startMonitoring();
    void stopMonitoring();
    void captureDark();
    void captureWhite();
    void exportSpectrumCsv();
    void exportCurrentImage();
    void handleWorkerState(DeviceWorker::State state, const QString &message);
    void handleSpectrum(const QVector<double> &wavelengths,
                        const QVector<double> &intensities,
                        const QDateTime &capturedAt);
    void handleCameraFrame(const QImage &image, const QDateTime &capturedAt);
    void handleWorkerError(const QString &message);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class SpectrumDisplayMode {
        RawIntensity,
        Reflectance,
        Transmittance
    };

    enum class SummarySemantic {
        Neutral,
        Success,
        Running,
        Warning,
        Offline,
        Disabled
    };

    DeviceProfile m_spectrometerProfile;
    DeviceProfile m_cameraProfile;

    QThread *m_spectrometerThread = nullptr;
    QThread *m_cameraThread = nullptr;
    DeviceWorker *m_spectrometerWorker = nullptr;
    DeviceWorker *m_cameraWorker = nullptr;
    DeviceWorker::State m_spectrometerState = DeviceWorker::State::Disconnected;
    DeviceWorker::State m_cameraState = DeviceWorker::State::Disconnected;
    QString m_spectrometerStateMessage;
    QString m_cameraStateMessage;
    DeviceWorker::State m_deviceState = DeviceWorker::State::Disconnected;

    QVector<double> m_lastWavelengths;
    QVector<double> m_lastRawIntensities;
    QVector<double> m_lastDisplayIntensities;
    QVector<double> m_lastReflectance;
    QVector<double> m_lastTransmittance;
    SpectrumReference m_darkReference;
    SpectrumReference m_whiteReference;
    SpectrumReference m_transmittanceDarkReference;
    SpectrumReference m_transmittanceReference;
    SpectrumDisplayMode m_spectrumDisplayMode = SpectrumDisplayMode::RawIntensity;
    bool m_lastSpectrumCalibrated = false;
    QImage m_lastImage;

    QLabel *m_headerLabel = nullptr;
    QLabel *m_deviceSummary = nullptr;
    QWidget *m_statusCard = nullptr;
    QLabel *m_statusSummary = nullptr;
    QWidget *m_peakCard = nullptr;
    QLabel *m_peakSummary = nullptr;
    QWidget *m_refreshCard = nullptr;
    QLabel *m_refreshSummary = nullptr;
    QWidget *m_frameCard = nullptr;
    QLabel *m_frameSummary = nullptr;
    QLabel *m_cameraView = nullptr;
    QWidget *m_cameraLoadingOverlay = nullptr;
    QLabel *m_cameraLoadingText = nullptr;
    QPlainTextEdit *m_logOutput = nullptr;
    SpectrumChartWidget *m_chartWidget = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_rawModeButton = nullptr;
    QPushButton *m_reflectanceModeButton = nullptr;
    QPushButton *m_transmittanceModeButton = nullptr;
    QPushButton *m_darkReferenceButton = nullptr;
    QPushButton *m_brightReferenceButton = nullptr;
    QSpinBox *m_integrationSpin = nullptr;
    QSpinBox *m_smoothingSpin = nullptr;
    QSpinBox *m_averageSpin = nullptr;
    QSpinBox *m_exposureSpin = nullptr;
    QSpinBox *m_gainSpin = nullptr;
    QLineEdit *m_sampleIdEdit = nullptr;
    QLineEdit *m_batchIdEdit = nullptr;
    QLineEdit *m_operatorEdit = nullptr;
    int m_cameraFrameCount = 0;
    QDateTime m_lastRefreshTime;
    QDateTime m_lastSpectrumTime;
    QDateTime m_lastCameraTime;
    bool m_closeConfirmed = false;
    bool m_autoStartMonitoring = false;

    void appendLog(const QString &message);
    void updateOverview();
    void updateActionState(bool connected);
    QWidget *buildJobPanel();
    QWidget *buildDetectorPanel();
    QWidget *buildSpectrumPanel();
    QWidget *buildCameraPanel();
    QWidget *createInfoCard(const QString &title, QLabel **valueLabel, QWidget **cardWidget);
    void applySemanticCardStyle(QWidget *card, QLabel *valueLabel, SummarySemantic semantic);
    static QString semanticKey(SummarySemantic semantic);
    void updateCameraPreview();
    void updateCameraWaitingState(DeviceWorker::State state, const QString &message);
    void setupDeviceWorkers();
    void setupDeviceWorker(bool spectrometer);
    void updateCombinedWorkerState(const QString &message);
    void shutdownDeviceWorker(QThread *thread, DeviceWorker *worker);
    void requestCurrentParameters();
    bool validateJobContext();
    void setSpectrumDisplayMode(SpectrumDisplayMode mode);
    void updateSpectrumModeUi();
    QString spectrumModeId() const;
    QString spectrumModeLabel() const;
    QString currentSpectrometerDeviceId() const;
    QString currentCameraDeviceId() const;
    QString currentExportDirectory() const;
    void rememberExportDirectory(const QString &fileName);
    void refreshCalibratedSpectrum();
    bool isSpectrometerMode() const;
    bool isCameraMode() const;
    bool isFusionMode() const;
    bool hasSpectrometer() const;
    bool hasCamera() const;
};
