#include "mainwindow.h"

#include "services/abstractcameraservice.h"
#include "services/abstractspectrometerservice.h"
#include "services/ebuscameraservice.h"
#include "services/mockcameraservice.h"
#include "services/mockspectrometerservice.h"
#include "services/oceandirectspectrometerservice.h"
#include "services/seaspectrometerservice.h"
#include "ui/apptheme.h"
#include "widgets/spectrumchartwidget.h"

#include <QAbstractSpinBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPixmap>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString subtleSectionTitleStyle()
{
    return QStringLiteral("font: 700 18pt 'Microsoft YaHei UI'; color: %1;").arg(AppTheme::textColor());
}

QString subtleSectionHintStyle()
{
    return QStringLiteral("font: 10.8pt 'Microsoft YaHei UI'; color: %1; line-height: 1.5;")
        .arg(AppTheme::hintTextColor());
}

void configureNumericInput(QSpinBox *spinBox, int minWidth)
{
    spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spinBox->setKeyboardTracking(false);
    spinBox->setAlignment(Qt::AlignCenter);
    spinBox->setMinimumWidth(minWidth);
    spinBox->setStyleSheet(AppTheme::spinBoxStyle());
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QPushButton *createToolbarButton(const QString &text, const QString &color, QWidget *parent)
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumHeight(58);
    button->setMinimumWidth(190);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setStyleSheet(AppTheme::primaryButtonStyle(color));
    return button;
}

} // namespace

MainWindow::MainWindow(const DeviceProfile &spectrometerProfile,
                       const DeviceProfile &cameraProfile,
                       QWidget *parent)
    : QMainWindow(parent)
    , m_spectrometerProfile(spectrometerProfile)
    , m_cameraProfile(cameraProfile)
{
    ensureServices();

    const QString pageTitle = isSpectrometerMode()
                                  ? QStringLiteral("%1检测界面").arg(m_spectrometerProfile.name)
                                  : QStringLiteral("%1检测界面").arg(m_cameraProfile.name);
    setWindowTitle(pageTitle);
    resize(1720, 1140);

    m_spectrumTimer = new QTimer(this);
    m_spectrumTimer->setInterval(900);
    connect(m_spectrumTimer, &QTimer::timeout, this, &MainWindow::refreshSpectrum);

    m_cameraTimer = new QTimer(this);
    m_cameraTimer->setInterval(180);
    connect(m_cameraTimer, &QTimer::timeout, this, &MainWindow::refreshCamera);

    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    auto *scrollArea = new QScrollArea(central);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { background: %1; border: none; }")
                                  .arg(AppTheme::surfaceColor()) +
                              AppTheme::verticalScrollBarStyle());

    auto *content = new QWidget(scrollArea);
    auto *rootLayout = new QVBoxLayout(content);
    rootLayout->setContentsMargins(30, 24, 30, 24);
    rootLayout->setSpacing(18);

    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(12);

    auto *returnButton = new QPushButton(QStringLiteral("返回首页"), content);
    returnButton->setMinimumHeight(54);
    returnButton->setMinimumWidth(170);
    returnButton->setStyleSheet(AppTheme::secondaryButtonStyle());
    connect(returnButton, &QPushButton::clicked, this, [this]() {
        if (m_spectrumTimer->isActive() || m_cameraTimer->isActive()) {
            const auto answer = QMessageBox::question(
                this,
                QStringLiteral("确认返回"),
                QStringLiteral("当前仍在采集或刷新中，是否停止当前检测并返回首页？"));
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        emit requestReturnToLauncher();
        close();
    });

    topBar->addWidget(returnButton, 0, Qt::AlignLeft);
    topBar->addStretch();
    rootLayout->addLayout(topBar);

    m_headerLabel = new QLabel(pageTitle, content);
    m_headerLabel->setStyleSheet(AppTheme::titleStyle(34));
    rootLayout->addWidget(m_headerLabel);

    m_deviceSummary = new QLabel(content);
    m_deviceSummary->setWordWrap(true);
    m_deviceSummary->setStyleSheet(AppTheme::bodyTextStyle());
    rootLayout->addWidget(m_deviceSummary);

    m_sdkSummary = new QLabel(content);
    m_sdkSummary->setWordWrap(true);
    m_sdkSummary->setStyleSheet(QStringLiteral("font: 12pt 'Microsoft YaHei UI'; color: %1;")
                                    .arg(AppTheme::secondaryTextColor()));
    rootLayout->addWidget(m_sdkSummary);

    auto *actionBar = new QHBoxLayout();
    actionBar->setSpacing(16);

    m_connectButton = createToolbarButton(QStringLiteral("连接设备"), QStringLiteral("#0f766e"), content);
    m_startButton = createToolbarButton(QStringLiteral("开始检测"), QStringLiteral("#2563eb"), content);
    m_stopButton = createToolbarButton(QStringLiteral("停止检测"), QStringLiteral("#b91c1c"), content);

    actionBar->addWidget(m_connectButton);
    actionBar->addWidget(m_startButton);
    actionBar->addWidget(m_stopButton);

    QPushButton *exportCsvButton = nullptr;
    QPushButton *exportImageButton = nullptr;

    if (isSpectrometerMode()) {
        exportCsvButton = createToolbarButton(QStringLiteral("导出光谱 CSV"), QStringLiteral("#7c3aed"), content);
        actionBar->addWidget(exportCsvButton);
    }
    if (isCameraMode()) {
        exportImageButton = createToolbarButton(QStringLiteral("保存当前图像"), QStringLiteral("#ea580c"), content);
        actionBar->addWidget(exportImageButton);
    }

    rootLayout->addLayout(actionBar);

    auto *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(14);
    cardsRow->addWidget(createInfoCard(QStringLiteral("当前状态"), &m_statusSummary, &m_statusCard));
    cardsRow->addWidget(createInfoCard(QStringLiteral("峰值 / 关键值"), &m_peakSummary, &m_peakCard));
    cardsRow->addWidget(createInfoCard(QStringLiteral("最近刷新"), &m_refreshSummary, &m_refreshCard));
    cardsRow->addWidget(createInfoCard(QStringLiteral("相机帧数"), &m_frameSummary, &m_frameCard));
    rootLayout->addLayout(cardsRow);

    rootLayout->addWidget(buildDetectorPanel(), 1);

    auto *logTitle = new QLabel(QStringLiteral("运行日志"), content);
    logTitle->setStyleSheet(QStringLiteral("font: 700 14pt 'Microsoft YaHei UI'; color: %1;")
                                .arg(AppTheme::secondaryTextColor()));
    rootLayout->addWidget(logTitle);

    m_logOutput = new QPlainTextEdit(content);
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumBlockCount(300);
    m_logOutput->setMinimumHeight(120);
    m_logOutput->setMaximumHeight(160);
    m_logOutput->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        " background: #0f172a;"
        " color: #e2e8f0;"
        " border: none;"
        " border-radius: 16px;"
        " padding: 12px 14px;"
        " font: 10.5pt Consolas;"
        " }"));
    rootLayout->addWidget(m_logOutput);

    content->setStyleSheet(QStringLiteral("background: %1;").arg(AppTheme::surfaceColor()));
    scrollArea->setWidget(content);
    centralLayout->addWidget(scrollArea);

    setCentralWidget(central);
    central->setStyleSheet(QStringLiteral("background: %1;").arg(AppTheme::surfaceColor()));

    connect(m_connectButton, &QPushButton::clicked, this, &MainWindow::connectDevices);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startMonitoring);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::stopMonitoring);
    if (exportCsvButton) {
        connect(exportCsvButton, &QPushButton::clicked, this, &MainWindow::exportSpectrumCsv);
    }
    if (exportImageButton) {
        connect(exportImageButton, &QPushButton::clicked, this, &MainWindow::exportCurrentImage);
    }

    updateOverview();
    updateActionState(false);
    appendLog(QStringLiteral("当前已进入独立检测界面，请先连接设备，再开始检测。"));
}

MainWindow::~MainWindow() = default;

QString MainWindow::semanticKey(SummarySemantic semantic)
{
    switch (semantic) {
    case SummarySemantic::Success:
        return QStringLiteral("success");
    case SummarySemantic::Running:
        return QStringLiteral("running");
    case SummarySemantic::Warning:
        return QStringLiteral("warning");
    case SummarySemantic::Offline:
        return QStringLiteral("offline");
    case SummarySemantic::Disabled:
        return QStringLiteral("disabled");
    case SummarySemantic::Neutral:
    default:
        return QStringLiteral("neutral");
    }
}

bool MainWindow::isSpectrometerMode() const
{
    return static_cast<bool>(m_spectrometer) && !m_camera;
}

bool MainWindow::isCameraMode() const
{
    return static_cast<bool>(m_camera) && !m_spectrometer;
}

void MainWindow::ensureServices()
{
    if (m_spectrometerProfile.id == QStringLiteral("ocean-direct")) {
        m_spectrometer.reset(new OceanDirectSpectrometerService());
    } else if (m_spectrometerProfile.id == QStringLiteral("oceanhood")) {
        m_spectrometer.reset(new SeaSpectrometerService());
    } else if (m_spectrometerProfile.id == QStringLiteral("sim-spec")) {
        m_spectrometer.reset(new MockSpectrometerService());
    }

    if (m_cameraProfile.id == QStringLiteral("ebus-camera")) {
        m_camera.reset(new EBusCameraService());
    } else if (m_cameraProfile.id == QStringLiteral("sim-camera")) {
        m_camera.reset(new MockCameraService());
    }
}

QWidget *MainWindow::createInfoCard(const QString &title, QLabel **valueLabel, QWidget **cardWidget)
{
    auto *card = new QWidget(this);
    card->setMinimumHeight(96);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    if (cardWidget) {
        *cardWidget = card;
    }

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(6);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(QStringLiteral("font: 700 10.5pt 'Microsoft YaHei UI'; color: %1;")
                                  .arg(AppTheme::secondaryTextColor()));
    layout->addWidget(titleLabel);

    *valueLabel = new QLabel(QStringLiteral("等待中"), card);
    (*valueLabel)->setWordWrap(true);
    (*valueLabel)->setStyleSheet(QStringLiteral("font: 700 12pt 'Microsoft YaHei UI'; color: %1;")
                                     .arg(AppTheme::textColor()));
    layout->addWidget(*valueLabel);

    applySemanticCardStyle(card, *valueLabel, SummarySemantic::Neutral);
    return card;
}

void MainWindow::applySemanticCardStyle(QWidget *card, QLabel *valueLabel, SummarySemantic semantic)
{
    if (!card || !valueLabel) {
        return;
    }

    const QString key = semanticKey(semantic);
    card->setStyleSheet(QStringLiteral(
                            "QWidget {"
                            " background: %1;"
                            " border: 1px solid %2;"
                            " border-radius: 18px;"
                            " }"
                            "QLabel { border: none; background: transparent; color: %3; }")
                            .arg(AppTheme::semanticBackgroundColor(key),
                                 AppTheme::semanticBorderColor(key),
                                 AppTheme::textColor()));
    valueLabel->setStyleSheet(QStringLiteral("font: 700 12pt 'Microsoft YaHei UI'; color: %1;")
                                  .arg(AppTheme::semanticTextColor(key)));
}

QWidget *MainWindow::buildDetectorPanel()
{
    return isSpectrometerMode() ? buildSpectrumPanel() : buildCameraPanel();
}

QWidget *MainWindow::buildSpectrumPanel()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);
    page->setMinimumHeight(1180);

    auto *controlsCard = new QWidget(page);
    controlsCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *controlsLayout = new QVBoxLayout(controlsCard);
    controlsLayout->setContentsMargins(26, 24, 26, 22);
    controlsLayout->setSpacing(16);

    auto *controlsTitle = new QLabel(QStringLiteral("光谱参数区"), controlsCard);
    controlsTitle->setStyleSheet(subtleSectionTitleStyle());
    controlsLayout->addWidget(controlsTitle);

    auto *controlsHint = new QLabel(QStringLiteral("这里保留采集前最常用的 3 个参数，便于快速调试。参数框支持直接输入数字，输入后按回车或点击其他区域即可生效。"), controlsCard);
    controlsHint->setWordWrap(true);
    controlsHint->setStyleSheet(subtleSectionHintStyle());
    controlsLayout->addWidget(controlsHint);

    auto *controlsGrid = new QGridLayout();
    controlsGrid->setHorizontalSpacing(22);
    controlsGrid->setVerticalSpacing(10);
    controlsGrid->setColumnStretch(1, 1);
    controlsGrid->setColumnStretch(3, 1);
    controlsGrid->setColumnStretch(5, 1);

    auto labelStyle = QStringLiteral("font: 11.5pt 'Microsoft YaHei UI'; color: %1;")
                          .arg(AppTheme::secondaryTextColor());

    m_integrationSpin = new QSpinBox(controlsCard);
    m_integrationSpin->setRange(1000, 1000000);
    m_integrationSpin->setValue(100000);
    m_integrationSpin->setSuffix(QStringLiteral(" us"));
    configureNumericInput(m_integrationSpin, 220);

    m_smoothingSpin = new QSpinBox(controlsCard);
    m_smoothingSpin->setRange(0, 20);
    m_smoothingSpin->setValue(5);
    configureNumericInput(m_smoothingSpin, 180);

    m_averageSpin = new QSpinBox(controlsCard);
    m_averageSpin->setRange(1, 50);
    m_averageSpin->setValue(5);
    configureNumericInput(m_averageSpin, 180);

    auto *integrationLabel = new QLabel(QStringLiteral("积分时间"), controlsCard);
    auto *smoothingLabel = new QLabel(QStringLiteral("平滑宽度"), controlsCard);
    auto *averageLabel = new QLabel(QStringLiteral("平均次数"), controlsCard);
    integrationLabel->setStyleSheet(labelStyle);
    smoothingLabel->setStyleSheet(labelStyle);
    averageLabel->setStyleSheet(labelStyle);

    controlsGrid->addWidget(integrationLabel, 0, 0);
    controlsGrid->addWidget(m_integrationSpin, 0, 1);
    controlsGrid->addWidget(smoothingLabel, 0, 2);
    controlsGrid->addWidget(m_smoothingSpin, 0, 3);
    controlsGrid->addWidget(averageLabel, 0, 4);
    controlsGrid->addWidget(m_averageSpin, 0, 5);
    controlsLayout->addLayout(controlsGrid);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(16);

    auto *darkButton = createToolbarButton(QStringLiteral("采集暗参考"), QStringLiteral("#0f766e"), controlsCard);
    auto *whiteButton = createToolbarButton(QStringLiteral("采集白参考"), QStringLiteral("#2563eb"), controlsCard);
    darkButton->setMinimumWidth(220);
    whiteButton->setMinimumWidth(220);
    buttonRow->addWidget(darkButton);
    buttonRow->addWidget(whiteButton);
    buttonRow->addStretch();
    controlsLayout->addLayout(buttonRow);

    connect(darkButton, &QPushButton::clicked, this, &MainWindow::captureDark);
    connect(whiteButton, &QPushButton::clicked, this, &MainWindow::captureWhite);

    auto *chartCard = new QWidget(page);
    chartCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(26, 24, 26, 24);
    chartLayout->setSpacing(10);

    auto *chartTitle = new QLabel(QStringLiteral("实时光谱区"), chartCard);
    chartTitle->setStyleSheet(subtleSectionTitleStyle());
    chartLayout->addWidget(chartTitle);

    auto *chartHint = new QLabel(QStringLiteral("图谱区作为主工作区，优先展示实时曲线。窗口较小时可通过滚动条查看完整图谱。"), chartCard);
    chartHint->setWordWrap(true);
    chartHint->setStyleSheet(subtleSectionHintStyle());
    chartLayout->addWidget(chartHint);

    m_chartWidget = new SpectrumChartWidget(chartCard);
    m_chartWidget->clear(QStringLiteral("暂未获取实时光谱。"));
    m_chartWidget->setMinimumHeight(780);
    chartLayout->addWidget(m_chartWidget, 1);

    layout->addWidget(controlsCard);
    layout->addWidget(chartCard, 2);
    return page;
}

QWidget *MainWindow::buildCameraPanel()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    page->setMinimumHeight(1060);

    auto *controlsCard = new QWidget(page);
    controlsCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *controlsLayout = new QVBoxLayout(controlsCard);
    controlsLayout->setContentsMargins(24, 22, 24, 22);
    controlsLayout->setSpacing(18);

    auto *controlsTitle = new QLabel(QStringLiteral("相机参数区"), controlsCard);
    controlsTitle->setStyleSheet(subtleSectionTitleStyle());
    controlsLayout->addWidget(controlsTitle);

    auto *controlsHint = new QLabel(QStringLiteral("参数框支持直接输入数字，适合在调试时快速修改曝光和增益。"), controlsCard);
    controlsHint->setWordWrap(true);
    controlsHint->setStyleSheet(subtleSectionHintStyle());
    controlsLayout->addWidget(controlsHint);

    auto *controlsGrid = new QGridLayout();
    controlsGrid->setHorizontalSpacing(20);
    controlsGrid->setVerticalSpacing(12);
    controlsGrid->setColumnStretch(1, 1);
    controlsGrid->setColumnStretch(3, 1);

    auto labelStyle = QStringLiteral("font: 11.5pt 'Microsoft YaHei UI'; color: %1;")
                          .arg(AppTheme::secondaryTextColor());

    m_exposureSpin = new QSpinBox(controlsCard);
    m_exposureSpin->setRange(1, 200);
    m_exposureSpin->setValue(12);
    m_exposureSpin->setSuffix(QStringLiteral(" ms"));
    configureNumericInput(m_exposureSpin, 220);

    m_gainSpin = new QSpinBox(controlsCard);
    m_gainSpin->setRange(1, 20);
    m_gainSpin->setValue(1);
    configureNumericInput(m_gainSpin, 220);

    auto *exposureLabel = new QLabel(QStringLiteral("曝光时间"), controlsCard);
    auto *gainLabel = new QLabel(QStringLiteral("增益"), controlsCard);
    exposureLabel->setStyleSheet(labelStyle);
    gainLabel->setStyleSheet(labelStyle);

    controlsGrid->addWidget(exposureLabel, 0, 0);
    controlsGrid->addWidget(m_exposureSpin, 0, 1);
    controlsGrid->addWidget(gainLabel, 0, 2);
    controlsGrid->addWidget(m_gainSpin, 0, 3);
    controlsLayout->addLayout(controlsGrid);

    auto *previewCard = new QWidget(page);
    previewCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(24, 22, 24, 22);
    previewLayout->setSpacing(12);

    auto *previewTitle = new QLabel(QStringLiteral("实时画面区"), previewCard);
    previewTitle->setStyleSheet(subtleSectionTitleStyle());
    previewLayout->addWidget(previewTitle);

    auto *previewHint = new QLabel(QStringLiteral("下方显示工业相机的实时图像。窗口高度不足时可滚动查看完整区域。"), previewCard);
    previewHint->setWordWrap(true);
    previewHint->setStyleSheet(subtleSectionHintStyle());
    previewLayout->addWidget(previewHint);

    m_cameraView = new QLabel(previewCard);
    m_cameraView->setMinimumHeight(760);
    m_cameraView->setAlignment(Qt::AlignCenter);
    m_cameraView->setStyleSheet(QStringLiteral(
        "QLabel {"
            " background: #f8fafc;"
            " border: 1px solid #dbe3ec;"
            " border-radius: 18px;"
            " color: #64748b;"
            " font: 12pt 'Microsoft YaHei UI';"
            " }"));
    m_cameraView->setText(QStringLiteral("暂未获取实时图像。"));
    previewLayout->addWidget(m_cameraView, 1);

    layout->addWidget(controlsCard);
    layout->addWidget(previewCard, 1);
    return page;
}

void MainWindow::appendLog(const QString &message)
{
    m_logOutput->appendPlainText(QStringLiteral("[%1] %2")
                                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), message));
}

void MainWindow::updateOverview()
{
    if (isSpectrometerMode()) {
        m_headerLabel->setText(m_spectrometerProfile.name);
        m_deviceSummary->setText(QStringLiteral("当前为独立光谱检测界面，仅显示 %1 对应的检测流程，不显示工业相机相关内容。")
                                     .arg(m_spectrometerProfile.name));
        m_sdkSummary->setText(QStringLiteral("波段范围：%1    使用 SDK：%2")
                                  .arg(m_spectrometerProfile.wavelengthRange, m_spectrometerProfile.sdkName));
        m_frameSummary->setText(QStringLiteral("不适用"));
        applySemanticCardStyle(m_frameCard, m_frameSummary, SummarySemantic::Disabled);
    } else if (isCameraMode()) {
        m_headerLabel->setText(m_cameraProfile.name);
        m_deviceSummary->setText(QStringLiteral("当前为独立工业相机检测界面，仅显示 %1 对应的检测流程，不显示光谱仪相关内容。")
                                     .arg(m_cameraProfile.name));
        m_sdkSummary->setText(QStringLiteral("使用 SDK：%1").arg(m_cameraProfile.sdkName));
        m_peakSummary->setText(QStringLiteral("不适用"));
        applySemanticCardStyle(m_peakCard, m_peakSummary, SummarySemantic::Disabled);
    }

    m_statusSummary->setText(QStringLiteral("等待连接"));
    applySemanticCardStyle(m_statusCard, m_statusSummary, SummarySemantic::Offline);
    if (m_refreshSummary->text().isEmpty() || m_refreshSummary->text() == QStringLiteral("等待中")) {
        m_refreshSummary->setText(QStringLiteral("暂无刷新"));
        applySemanticCardStyle(m_refreshCard, m_refreshSummary, SummarySemantic::Warning);
    }
    if (m_peakSummary->text().isEmpty() || m_peakSummary->text() == QStringLiteral("等待中")) {
        m_peakSummary->setText(QStringLiteral("暂无实时数据"));
        applySemanticCardStyle(m_peakCard, m_peakSummary, SummarySemantic::Warning);
    }
    if (m_frameSummary->text().isEmpty() || m_frameSummary->text() == QStringLiteral("等待中")) {
        m_frameSummary->setText(QStringLiteral("0 帧"));
        applySemanticCardStyle(m_frameCard, m_frameSummary, SummarySemantic::Warning);
    }
}

void MainWindow::updateActionState(bool connected)
{
    m_startButton->setEnabled(connected);
    m_stopButton->setEnabled(connected);
}

void MainWindow::connectDevices()
{
    bool anythingConnected = false;

    if (m_spectrometer) {
        QString errorMessage;
        m_spectrometer->setIntegrationTimeUs(m_integrationSpin ? m_integrationSpin->value() : 100000);
        m_spectrometer->setSmoothing(m_smoothingSpin ? m_smoothingSpin->value() : 5);
        m_spectrometer->setAverageCount(m_averageSpin ? m_averageSpin->value() : 5);

        if (m_spectrometer->open(&errorMessage)) {
            appendLog(QStringLiteral("光谱仪连接成功：%1").arg(m_spectrometer->serviceName()));
            m_deviceSummary->setText(m_spectrometer->deviceSummary());
            anythingConnected = true;
        } else {
            appendLog(QStringLiteral("光谱仪连接失败：%1").arg(errorMessage));
            m_deviceSummary->setText(QStringLiteral("光谱仪未连接：%1").arg(errorMessage));
        }
    }

    if (m_camera) {
        QString errorMessage;
        m_camera->setExposureMs(m_exposureSpin ? m_exposureSpin->value() : 12);
        m_camera->setGain(m_gainSpin ? m_gainSpin->value() : 1);
        if (m_camera->open(&errorMessage)) {
            appendLog(QStringLiteral("工业相机连接成功：%1").arg(m_camera->serviceName()));
            m_deviceSummary->setText(m_camera->deviceSummary());
            anythingConnected = true;
        } else {
            appendLog(QStringLiteral("工业相机连接失败：%1").arg(errorMessage));
            m_deviceSummary->setText(QStringLiteral("工业相机未连接：%1").arg(errorMessage));
        }
    }

    m_statusSummary->setText(anythingConnected ? QStringLiteral("已连接，可开始检测")
                                               : QStringLiteral("连接未完成"));
    applySemanticCardStyle(m_statusCard,
                           m_statusSummary,
                           anythingConnected ? SummarySemantic::Success : SummarySemantic::Offline);
    updateActionState(anythingConnected);
}

void MainWindow::startMonitoring()
{
    bool started = false;

    if (m_spectrometer && m_spectrometer->isOpen()) {
        m_spectrometer->setIntegrationTimeUs(m_integrationSpin->value());
        m_spectrometer->setSmoothing(m_smoothingSpin->value());
        m_spectrometer->setAverageCount(m_averageSpin->value());
        m_spectrumTimer->start();
        started = true;
    }

    if (m_camera && m_camera->isOpen()) {
        m_camera->setExposureMs(m_exposureSpin->value());
        m_camera->setGain(m_gainSpin->value());
        m_cameraTimer->start();
        started = true;
    }

    if (!started) {
        QMessageBox::warning(this, QStringLiteral("无法开始"), QStringLiteral("请先成功连接当前检测界面的设备。"));
        return;
    }

    appendLog(QStringLiteral("检测已开始。"));
    m_statusSummary->setText(QStringLiteral("检测中"));
    applySemanticCardStyle(m_statusCard, m_statusSummary, SummarySemantic::Running);
}

void MainWindow::stopMonitoring()
{
    m_spectrumTimer->stop();
    m_cameraTimer->stop();
    appendLog(QStringLiteral("检测已停止。"));
    m_statusSummary->setText(QStringLiteral("检测已暂停"));
    applySemanticCardStyle(m_statusCard, m_statusSummary, SummarySemantic::Warning);
}

void MainWindow::refreshSpectrum()
{
    if (!m_spectrometer || !m_spectrometer->isOpen()) {
        return;
    }

    QString errorMessage;
    if (!m_spectrometer->acquire(m_lastWavelengths, m_lastIntensities, &errorMessage)) {
        appendLog(QStringLiteral("光谱刷新失败：%1").arg(errorMessage));
        return;
    }

    int peakIndex = 0;
    double peakValue = m_lastIntensities.value(0);
    for (int i = 1; i < m_lastIntensities.size(); ++i) {
        if (m_lastIntensities.at(i) > peakValue) {
            peakValue = m_lastIntensities.at(i);
            peakIndex = i;
        }
    }

    m_chartWidget->setSeries(m_lastWavelengths,
                             m_lastIntensities,
                             QStringLiteral("实时光谱 - %1").arg(m_spectrometer->serviceName()),
                             QColor(QStringLiteral("#2563eb")));
    m_peakSummary->setText(QStringLiteral("峰值：%1 nm / %2")
                               .arg(m_lastWavelengths.value(peakIndex), 0, 'f', 1)
                               .arg(peakValue, 0, 'f', 1));
    applySemanticCardStyle(m_peakCard, m_peakSummary, SummarySemantic::Running);
    m_lastRefreshTime = QDateTime::currentDateTime();
    m_refreshSummary->setText(m_lastRefreshTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    applySemanticCardStyle(m_refreshCard, m_refreshSummary, SummarySemantic::Success);
}

void MainWindow::refreshCamera()
{
    if (!m_camera || !m_camera->isOpen()) {
        return;
    }

    m_camera->setExposureMs(m_exposureSpin->value());
    m_camera->setGain(m_gainSpin->value());
    m_lastImage = m_camera->grabFrame();
    if (m_lastImage.isNull()) {
        appendLog(QStringLiteral("相机未返回有效图像。"));
        return;
    }

    ++m_cameraFrameCount;
    updateCameraPreview();
    m_frameSummary->setText(QStringLiteral("%1 帧").arg(m_cameraFrameCount));
    applySemanticCardStyle(m_frameCard, m_frameSummary, SummarySemantic::Running);
    m_lastRefreshTime = QDateTime::currentDateTime();
    m_refreshSummary->setText(m_lastRefreshTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    applySemanticCardStyle(m_refreshCard, m_refreshSummary, SummarySemantic::Success);
}

void MainWindow::updateCameraPreview()
{
    if (!m_cameraView || m_lastImage.isNull()) {
        return;
    }

    m_cameraView->setPixmap(QPixmap::fromImage(m_lastImage).scaled(
        m_cameraView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::captureDark()
{
    if (m_lastIntensities.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("暂无光谱"), QStringLiteral("请先获取实时光谱，再采集暗参考。"));
        return;
    }

    m_darkReference = m_lastIntensities;
    appendLog(QStringLiteral("暗参考已采集。"));
}

void MainWindow::captureWhite()
{
    if (m_lastIntensities.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("暂无光谱"), QStringLiteral("请先获取实时光谱，再采集白参考。"));
        return;
    }

    m_whiteReference = m_lastIntensities;
    appendLog(QStringLiteral("白参考已采集。"));
}

void MainWindow::exportSpectrumCsv()
{
    if (m_lastWavelengths.isEmpty() || m_lastIntensities.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("暂无数据"), QStringLiteral("当前没有可导出的光谱数据。"));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出光谱 CSV"),
        QStringLiteral("光谱导出.csv"),
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("无法打开目标文件。"));
        return;
    }

    QTextStream stream(&file);
    stream << "wavelength_nm,intensity,dark_reference,white_reference\n";
    for (int i = 0; i < m_lastWavelengths.size(); ++i) {
        stream << QString::number(m_lastWavelengths.at(i), 'f', 4) << ','
               << QString::number(m_lastIntensities.at(i), 'f', 4) << ','
               << QString::number(m_darkReference.value(i), 'f', 4) << ','
               << QString::number(m_whiteReference.value(i), 'f', 4) << '\n';
    }

    appendLog(QStringLiteral("光谱 CSV 已导出到：%1").arg(fileName));
}

void MainWindow::exportCurrentImage()
{
    if (m_lastImage.isNull()) {
        QMessageBox::warning(this, QStringLiteral("暂无图像"), QStringLiteral("当前没有可导出的相机图像。"));
        return;
    }

    const QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存当前图像"),
        QStringLiteral("相机快照.png"),
        QStringLiteral("PNG 文件 (*.png);;JPEG 文件 (*.jpg *.jpeg)"));
    if (fileName.isEmpty()) {
        return;
    }

    QImageWriter writer(fileName);
    if (!writer.write(m_lastImage)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), writer.errorString());
        return;
    }

    appendLog(QStringLiteral("相机图像已保存到：%1").arg(fileName));
}
