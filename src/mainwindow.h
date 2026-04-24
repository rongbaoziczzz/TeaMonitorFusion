#pragma once

#include "models/devicedefinitions.h"

#include <QDateTime>
#include <QImage>
#include <QMainWindow>
#include <QScopedPointer>
#include <QVector>

class AbstractCameraService;
class AbstractSpectrometerService;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
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

signals:
    void requestReturnToLauncher();

private slots:
    void connectDevices();
    void startMonitoring();
    void stopMonitoring();
    void refreshSpectrum();
    void refreshCamera();
    void captureDark();
    void captureWhite();
    void exportSpectrumCsv();
    void exportCurrentImage();

private:
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

    QScopedPointer<AbstractSpectrometerService> m_spectrometer;
    QScopedPointer<AbstractCameraService> m_camera;

    QTimer *m_spectrumTimer = nullptr;
    QTimer *m_cameraTimer = nullptr;

    QVector<double> m_lastWavelengths;
    QVector<double> m_lastIntensities;
    QVector<double> m_darkReference;
    QVector<double> m_whiteReference;
    QImage m_lastImage;

    QLabel *m_headerLabel = nullptr;
    QLabel *m_deviceSummary = nullptr;
    QLabel *m_sdkSummary = nullptr;
    QWidget *m_statusCard = nullptr;
    QLabel *m_statusSummary = nullptr;
    QWidget *m_peakCard = nullptr;
    QLabel *m_peakSummary = nullptr;
    QWidget *m_refreshCard = nullptr;
    QLabel *m_refreshSummary = nullptr;
    QWidget *m_frameCard = nullptr;
    QLabel *m_frameSummary = nullptr;
    QLabel *m_cameraView = nullptr;
    QPlainTextEdit *m_logOutput = nullptr;
    SpectrumChartWidget *m_chartWidget = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QSpinBox *m_integrationSpin = nullptr;
    QSpinBox *m_smoothingSpin = nullptr;
    QSpinBox *m_averageSpin = nullptr;
    QSpinBox *m_exposureSpin = nullptr;
    QSpinBox *m_gainSpin = nullptr;
    int m_cameraFrameCount = 0;
    QDateTime m_lastRefreshTime;

    void appendLog(const QString &message);
    void updateOverview();
    void updateActionState(bool connected);
    QWidget *buildDetectorPanel();
    QWidget *buildSpectrumPanel();
    QWidget *buildCameraPanel();
    QWidget *createInfoCard(const QString &title, QLabel **valueLabel, QWidget **cardWidget);
    void applySemanticCardStyle(QWidget *card, QLabel *valueLabel, SummarySemantic semantic);
    static QString semanticKey(SummarySemantic semantic);
    void updateCameraPreview();
    void ensureServices();
    bool isSpectrometerMode() const;
    bool isCameraMode() const;
};
