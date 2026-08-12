#include "mainwindow.h"

#include "services/filelogger.h"
#include "services/auditstore.h"
#include "ui/apptheme.h"
#include "widgets/spectrumchartwidget.h"

#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImageWriter>
#include <QPainter>
#include <QStackedLayout>
#include <QTimer>
#include <QKeySequence>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QPixmap>
#include <QSaveFile>
#include <QShortcut>
#include <QScrollArea>
#include <QSettings>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTextStream>
#include <QThread>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
class NoWheelSpinBox final : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        event->ignore();
    }
};

class BusyRingWidget final : public QWidget
{
public:
    explicit BusyRingWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(48, 48);
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 30) % 360;
            update();
        });
        timer->start(70);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF ringRect = QRectF(6, 6, width() - 12, height() - 12);
        for (int index = 0; index < 12; ++index) {
            QPen pen(QColor(15, 118, 110, 35 + index * 18));
            pen.setWidthF(4.0);
            pen.setCapStyle(Qt::RoundCap);
            painter.setPen(pen);
            painter.drawArc(ringRect, (m_angle - index * 30) * 16, 20 * 16);
        }
    }

private:
    int m_angle = 0;
};

QString subtleSectionTitleStyle()
{
    return QStringLiteral("font: 700 18pt 'Microsoft YaHei UI'; color: %1;").arg(AppTheme::textColor());
}

QString spectrumModeButtonStyle()
{
    return QStringLiteral(
        "QPushButton {"
        " min-height: 38px; padding: 0 18px;"
        " border: 1px solid #cbd5e1; border-radius: 5px;"
        " background: #f8fafc; color: #334155;"
        " font: 700 10.5pt 'Microsoft YaHei UI';"
        " }"
        "QPushButton:hover { background: #eef2f7; border-color: #94a3b8; }"
        "QPushButton:checked { background: #0f766e; border-color: #0f766e; color: white; }"
        "QPushButton:focus { border: 2px solid #38bdf8; }");
}

QString safeFilePart(QString value)
{
    value = value.trimmed();
    for (qsizetype i = 0; i < value.size(); ++i) {
        const QChar character = value.at(i);
        if (!character.isLetterOrNumber() && character != QLatin1Char('-') && character != QLatin1Char('_')) {
            value[i] = QLatin1Char('_');
        }
    }
    return value.isEmpty() ? QStringLiteral("unknown") : value;
}

QString csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(value);
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

QPushButton *createToolbarButton(const QString &text,
                                 const QString &color,
                                 QWidget *parent,
                                 QStyle::StandardPixmap icon = QStyle::SP_CustomBase)
{
    auto *button = new QPushButton(text, parent);
    button->setMinimumHeight(46);
    button->setMinimumWidth(150);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setCursor(Qt::PointingHandCursor);
    button->setAccessibleName(text);
    button->setToolTip(text);
    button->setStyleSheet(AppTheme::primaryButtonStyle(color));
    if (icon != QStyle::SP_CustomBase) {
        button->setIcon(AppTheme::tintedStandardIcon(parent->style(), icon));
    }
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
    const QString pageTitle = isFusionMode()
                                  ? QStringLiteral("光谱与图像融合检测界面")
                              : hasSpectrometer()
                                  ? QStringLiteral("%1检测界面").arg(m_spectrometerProfile.name)
                                  : QStringLiteral("%1检测界面").arg(m_cameraProfile.name);
    setWindowTitle(pageTitle);
    resize(1480, 960);
    setMinimumSize(980, 680);

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
    rootLayout->setContentsMargins(24, 20, 24, 20);
    rootLayout->setSpacing(14);

    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(12);

    auto *returnButton = new QPushButton(QStringLiteral("返回首页"), content);
    returnButton->setMinimumHeight(42);
    returnButton->setMinimumWidth(126);
    returnButton->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
    returnButton->setAccessibleName(QStringLiteral("返回设备入口"));
    returnButton->setToolTip(QStringLiteral("返回设备入口"));
    returnButton->setCursor(Qt::PointingHandCursor);
    returnButton->setStyleSheet(AppTheme::secondaryButtonStyle());
    connect(returnButton, &QPushButton::clicked, this, [this]() {
        if (m_deviceState == DeviceWorker::State::Monitoring ||
            m_deviceState == DeviceWorker::State::Reconnecting) {
            const auto answer = QMessageBox::question(
                this,
                QStringLiteral("确认返回"),
                QStringLiteral("当前仍在采集或刷新中，是否停止当前检测并返回首页？"));
            if (answer != QMessageBox::Yes) {
                return;
            }
        }
        m_closeConfirmed = true;
        emit requestReturnToLauncher();
        close();
    });

    topBar->addWidget(returnButton, 0, Qt::AlignLeft);
    topBar->addStretch();
    rootLayout->addLayout(topBar);

    m_headerLabel = new QLabel(pageTitle, content);
    m_headerLabel->setStyleSheet(AppTheme::titleStyle(28));
    rootLayout->addWidget(m_headerLabel);

    m_deviceSummary = new QLabel(content);
    m_deviceSummary->setWordWrap(true);
    m_deviceSummary->setStyleSheet(QStringLiteral(
        "font: 11pt 'Microsoft YaHei UI'; color: #334155;"
        " background: #ffffff; border: 1px solid #dbe3ec; border-radius: 6px;"
        " padding: 10px 12px;"));
    rootLayout->addWidget(m_deviceSummary);
    rootLayout->addWidget(buildJobPanel());

    auto *actionBar = new QHBoxLayout();
    actionBar->setSpacing(16);

    m_connectButton = createToolbarButton(QStringLiteral("连接设备"), QStringLiteral("#0f766e"), content,
                                          QStyle::SP_DialogApplyButton);
    m_startButton = createToolbarButton(QStringLiteral("开始检测"), QStringLiteral("#2563eb"), content,
                                        QStyle::SP_MediaPlay);
    m_stopButton = createToolbarButton(QStringLiteral("停止检测"), QStringLiteral("#b91c1c"), content,
                                       QStyle::SP_MediaStop);

    actionBar->addWidget(m_connectButton);
    actionBar->addWidget(m_startButton);
    actionBar->addWidget(m_stopButton);

    QPushButton *exportCsvButton = nullptr;
    QPushButton *exportImageButton = nullptr;

    if (hasSpectrometer()) {
        exportCsvButton = createToolbarButton(QStringLiteral("导出光谱 CSV"), QStringLiteral("#6d28d9"), content,
                                              QStyle::SP_DialogSaveButton);
        actionBar->addWidget(exportCsvButton);
    }
    if (hasCamera()) {
        exportImageButton = createToolbarButton(QStringLiteral("保存当前图像"), QStringLiteral("#c2410c"), content,
                                                QStyle::SP_DialogSaveButton);
        actionBar->addWidget(exportImageButton);
    }

    rootLayout->addLayout(actionBar);

    auto *cardsRow = new QHBoxLayout();
    cardsRow->setSpacing(14);
    cardsRow->addWidget(createInfoCard(QStringLiteral("当前状态"), &m_statusSummary, &m_statusCard));
    cardsRow->addWidget(createInfoCard(QStringLiteral("峰值 / 关键值"), &m_peakSummary, &m_peakCard));
    cardsRow->addWidget(createInfoCard(QStringLiteral("最近刷新"), &m_refreshSummary, &m_refreshCard));
    cardsRow->addWidget(createInfoCard(QStringLiteral("相机帧数"), &m_frameSummary, &m_frameCard));
    m_peakCard->setVisible(hasSpectrometer());
    m_frameCard->setVisible(hasCamera());
    rootLayout->addLayout(cardsRow);

    rootLayout->addWidget(buildDetectorPanel(), 1);

    auto *logTitle = new QLabel(QStringLiteral("运行日志"), content);
    logTitle->setStyleSheet(QStringLiteral("font: 700 14pt 'Microsoft YaHei UI'; color: %1;")
                                .arg(AppTheme::secondaryTextColor()));
    rootLayout->addWidget(logTitle);

    m_logOutput = new QPlainTextEdit(content);
    m_logOutput->setReadOnly(true);
    m_logOutput->setMaximumBlockCount(300);
    m_logOutput->setMinimumHeight(96);
    m_logOutput->setMaximumHeight(130);
    m_logOutput->setStyleSheet(QStringLiteral(
        "QPlainTextEdit {"
        " background: #0f172a;"
        " color: #e2e8f0;"
        " border: none;"
        " border-radius: 8px;"
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

    auto *connectShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this);
    connect(connectShortcut, &QShortcut::activated, this, &MainWindow::connectDevices);
    auto *startShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Return")), this);
    connect(startShortcut, &QShortcut::activated, this, &MainWindow::startMonitoring);
    auto *stopShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(stopShortcut, &QShortcut::activated, this, &MainWindow::stopMonitoring);
    statusBar()->showMessage(QStringLiteral("就绪 · 等待设备连接"));

    updateOverview();
    updateActionState(false);
    setupDeviceWorkers();
    appendLog(QStringLiteral("请连接设备后开始检测。"));
}

MainWindow::~MainWindow()
{
    shutdownDeviceWorker(m_spectrometerThread, m_spectrometerWorker);
    shutdownDeviceWorker(m_cameraThread, m_cameraWorker);
}

void MainWindow::startCameraLivePreview()
{
    if (!isCameraMode()) {
        return;
    }
    if (m_sampleIdEdit && m_sampleIdEdit->text().trimmed().isEmpty()) {
        m_sampleIdEdit->setText(QStringLiteral("LIVE"));
    }
    if (m_exposureSpin) {
        m_exposureSpin->setValue(12);
    }
    if (m_gainSpin && m_gainSpin->value() < 5) {
        m_gainSpin->setValue(5);
    }
    m_autoStartMonitoring = true;
    requestCurrentParameters();
    emit connectDeviceRequested();
}

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
    return hasSpectrometer() && !hasCamera();
}

bool MainWindow::isCameraMode() const
{
    return hasCamera() && !hasSpectrometer();
}

bool MainWindow::isFusionMode() const
{
    return hasSpectrometer() && hasCamera();
}

bool MainWindow::hasSpectrometer() const
{
    return m_spectrometerProfile.id != QStringLiteral("none");
}

bool MainWindow::hasCamera() const
{
    return m_cameraProfile.id != QStringLiteral("none");
}

QWidget *MainWindow::createInfoCard(const QString &title, QLabel **valueLabel, QWidget **cardWidget)
{
    auto *card = new QWidget(this);
    card->setMinimumHeight(84);
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
                            " border-radius: 8px;"
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
    if (isSpectrometerMode()) {
        return buildSpectrumPanel();
    }
    if (isCameraMode()) {
        return buildCameraPanel();
    }

    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(20);
    layout->addWidget(buildSpectrumPanel());
    layout->addWidget(buildCameraPanel());
    return page;
}

QWidget *MainWindow::buildJobPanel()
{
    auto *panel = new QWidget(this);
    panel->setStyleSheet(AppTheme::sectionCardStyle());
    auto *layout = new QGridLayout(panel);
    layout->setContentsMargins(20, 16, 20, 16);
    layout->setHorizontalSpacing(14);
    layout->setVerticalSpacing(8);

    auto *title = new QLabel(QStringLiteral("检测任务"), panel);
    title->setStyleSheet(QStringLiteral("font: 700 12pt 'Microsoft YaHei UI'; color: %1;")
                             .arg(AppTheme::textColor()));
    layout->addWidget(title, 0, 0, 1, 6);

    const QString editStyle = QStringLiteral(
        "QLineEdit { background: white; border: 1px solid #cbd5e1; border-radius: 6px;"
        " padding: 0 10px; min-height: 38px; font: 10.5pt 'Microsoft YaHei UI'; }"
        "QLineEdit:focus { border-color: #2563eb; }");
    m_sampleIdEdit = new QLineEdit(panel);
    m_batchIdEdit = new QLineEdit(panel);
    m_operatorEdit = new QLineEdit(panel);
    m_sampleIdEdit->setPlaceholderText(QStringLiteral("必填"));
    m_batchIdEdit->setPlaceholderText(QStringLiteral("可选"));
    m_operatorEdit->setPlaceholderText(QStringLiteral("建议填写"));
    m_sampleIdEdit->setMaxLength(64);
    m_batchIdEdit->setMaxLength(64);
    m_operatorEdit->setMaxLength(32);
    m_sampleIdEdit->setClearButtonEnabled(true);
    m_batchIdEdit->setClearButtonEnabled(true);
    m_operatorEdit->setClearButtonEnabled(true);
    m_sampleIdEdit->setAccessibleName(QStringLiteral("样品编号"));
    m_batchIdEdit->setAccessibleName(QStringLiteral("批次"));
    m_operatorEdit->setAccessibleName(QStringLiteral("操作员"));
    m_sampleIdEdit->setStyleSheet(editStyle);
    m_batchIdEdit->setStyleSheet(editStyle);
    m_operatorEdit->setStyleSheet(editStyle);

    QSettings settings;
    m_batchIdEdit->setText(settings.value(QStringLiteral("job/lastBatchId")).toString());
    m_operatorEdit->setText(settings.value(QStringLiteral("job/operator")).toString());

    layout->addWidget(new QLabel(QStringLiteral("样品编号 *"), panel), 1, 0);
    layout->addWidget(m_sampleIdEdit, 1, 1);
    layout->addWidget(new QLabel(QStringLiteral("批次"), panel), 1, 2);
    layout->addWidget(m_batchIdEdit, 1, 3);
    layout->addWidget(new QLabel(QStringLiteral("操作员"), panel), 1, 4);
    layout->addWidget(m_operatorEdit, 1, 5);
    layout->setColumnStretch(1, 2);
    layout->setColumnStretch(3, 2);
    layout->setColumnStretch(5, 2);

    connect(m_batchIdEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings().setValue(QStringLiteral("job/lastBatchId"), m_batchIdEdit->text().trimmed());
    });
    connect(m_operatorEdit, &QLineEdit::editingFinished, this, [this]() {
        QSettings().setValue(QStringLiteral("job/operator"), m_operatorEdit->text().trimmed());
    });
    return panel;
}

QWidget *MainWindow::buildSpectrumPanel()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *controlsCard = new QWidget(page);
    controlsCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *controlsLayout = new QVBoxLayout(controlsCard);
    controlsLayout->setContentsMargins(20, 18, 20, 18);
    controlsLayout->setSpacing(12);

    auto *controlsTitle = new QLabel(QStringLiteral("光谱采集参数"), controlsCard);
    controlsTitle->setStyleSheet(subtleSectionTitleStyle());
    controlsLayout->addWidget(controlsTitle);

    auto *modeRow = new QHBoxLayout();
    modeRow->setSpacing(8);
    auto *modeLabel = new QLabel(QStringLiteral("光谱模式"), controlsCard);
    modeLabel->setStyleSheet(QStringLiteral("font: 700 11.5pt 'Microsoft YaHei UI'; color: %1;")
                                 .arg(AppTheme::secondaryTextColor()));
    modeRow->addWidget(modeLabel);
    auto *modeGroup = new QButtonGroup(controlsCard);
    modeGroup->setExclusive(true);
    m_rawModeButton = new QPushButton(QStringLiteral("光照强度"), controlsCard);
    m_reflectanceModeButton = new QPushButton(QStringLiteral("反射率"), controlsCard);
    m_transmittanceModeButton = new QPushButton(QStringLiteral("透射率"), controlsCard);
    for (QPushButton *button : {m_rawModeButton, m_reflectanceModeButton, m_transmittanceModeButton}) {
        button->setCheckable(true);
        button->setMinimumWidth(118);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(spectrumModeButtonStyle());
        modeRow->addWidget(button);
        modeGroup->addButton(button);
    }
    m_rawModeButton->setChecked(true);
    modeRow->addStretch();
    controlsLayout->addLayout(modeRow);

    connect(m_rawModeButton, &QPushButton::clicked, this, [this]() {
        setSpectrumDisplayMode(SpectrumDisplayMode::RawIntensity);
    });
    connect(m_reflectanceModeButton, &QPushButton::clicked, this, [this]() {
        setSpectrumDisplayMode(SpectrumDisplayMode::Reflectance);
    });
    connect(m_transmittanceModeButton, &QPushButton::clicked, this, [this]() {
        setSpectrumDisplayMode(SpectrumDisplayMode::Transmittance);
    });

    auto *controlsGrid = new QGridLayout();
    controlsGrid->setHorizontalSpacing(22);
    controlsGrid->setVerticalSpacing(10);
    controlsGrid->setColumnStretch(1, 1);
    controlsGrid->setColumnStretch(3, 1);
    controlsGrid->setColumnStretch(5, 1);

    auto labelStyle = QStringLiteral("font: 11.5pt 'Microsoft YaHei UI'; color: %1;")
                          .arg(AppTheme::secondaryTextColor());

    m_integrationSpin = new NoWheelSpinBox(controlsCard);
    m_integrationSpin->setRange(1000, 1000000);
    QSettings settings;
    const QString settingsPrefix = QStringLiteral("spectrometer/%1/").arg(m_spectrometerProfile.id);
    const int defaultIntegrationTimeUs = m_spectrometerProfile.id == QStringLiteral("oceanhood") ? 1000 : 100000;
    m_integrationSpin->setValue(
        settings.value(settingsPrefix + QStringLiteral("integrationTimeUs"), defaultIntegrationTimeUs).toInt());
    m_integrationSpin->setSuffix(QStringLiteral(" us"));
    configureNumericInput(m_integrationSpin, 220);

    m_smoothingSpin = new NoWheelSpinBox(controlsCard);
    m_smoothingSpin->setRange(0, 20);
    m_smoothingSpin->setValue(settings.value(settingsPrefix + QStringLiteral("smoothing"), 5).toInt());
    configureNumericInput(m_smoothingSpin, 180);

    m_averageSpin = new NoWheelSpinBox(controlsCard);
    m_averageSpin->setRange(1, 50);
    m_averageSpin->setValue(settings.value(settingsPrefix + QStringLiteral("averageCount"), 5).toInt());
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

    m_darkReferenceButton = createToolbarButton(QStringLiteral("采集暗参考"), QStringLiteral("#0f766e"), controlsCard);
    m_brightReferenceButton = createToolbarButton(QStringLiteral("采集白参考"), QStringLiteral("#2563eb"), controlsCard);
    m_darkReferenceButton->setObjectName(QStringLiteral("captureDarkButton"));
    m_brightReferenceButton->setObjectName(QStringLiteral("captureBrightReferenceButton"));
    m_darkReferenceButton->setToolTip(QStringLiteral("将当前实时光谱保存为当前模式的暗参考"));
    m_brightReferenceButton->setToolTip(QStringLiteral("将当前实时光谱保存为当前模式的亮参考"));
    m_darkReferenceButton->setMinimumWidth(220);
    m_brightReferenceButton->setMinimumWidth(220);
    buttonRow->addWidget(m_darkReferenceButton);
    buttonRow->addWidget(m_brightReferenceButton);
    buttonRow->addStretch();
    controlsLayout->addLayout(buttonRow);

    connect(m_darkReferenceButton, &QPushButton::clicked, this, &MainWindow::captureDark);
    connect(m_brightReferenceButton, &QPushButton::clicked, this, &MainWindow::captureWhite);
    updateSpectrumModeUi();
    connect(m_integrationSpin, &QSpinBox::valueChanged, this, [settingsPrefix](int value) {
        QSettings().setValue(settingsPrefix + QStringLiteral("integrationTimeUs"), value);
    });
    connect(m_smoothingSpin, &QSpinBox::valueChanged, this, [settingsPrefix](int value) {
        QSettings().setValue(settingsPrefix + QStringLiteral("smoothing"), value);
    });
    connect(m_averageSpin, &QSpinBox::valueChanged, this, [settingsPrefix](int value) {
        QSettings().setValue(settingsPrefix + QStringLiteral("averageCount"), value);
    });
    connect(m_integrationSpin, &QSpinBox::valueChanged, this, [this](int) {
        if (m_deviceState == DeviceWorker::State::Connected || m_deviceState == DeviceWorker::State::Monitoring) {
            requestCurrentParameters();
        }
    });
    connect(m_smoothingSpin, &QSpinBox::valueChanged, this, [this](int) {
        if (m_deviceState == DeviceWorker::State::Connected || m_deviceState == DeviceWorker::State::Monitoring) {
            requestCurrentParameters();
        }
    });
    connect(m_averageSpin, &QSpinBox::valueChanged, this, [this](int) {
        if (m_deviceState == DeviceWorker::State::Connected || m_deviceState == DeviceWorker::State::Monitoring) {
            requestCurrentParameters();
        }
    });

    auto *chartCard = new QWidget(page);
    chartCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(26, 24, 26, 24);
    chartLayout->setSpacing(10);

    auto *chartTitle = new QLabel(QStringLiteral("实时光谱区"), chartCard);
    chartTitle->setStyleSheet(subtleSectionTitleStyle());
    chartLayout->addWidget(chartTitle);

    m_chartWidget = new SpectrumChartWidget(chartCard);
    m_chartWidget->clear(QStringLiteral("暂未获取实时光谱。"));
    m_chartWidget->setMinimumHeight(500);
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
    layout->setSpacing(12);

    auto *controlsCard = new QWidget(page);
    controlsCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *controlsLayout = new QVBoxLayout(controlsCard);
    controlsLayout->setContentsMargins(20, 18, 20, 18);
    controlsLayout->setSpacing(12);

    auto *controlsTitle = new QLabel(QStringLiteral("相机采集参数"), controlsCard);
    controlsTitle->setStyleSheet(subtleSectionTitleStyle());
    controlsLayout->addWidget(controlsTitle);

    auto *controlsGrid = new QGridLayout();
    controlsGrid->setHorizontalSpacing(20);
    controlsGrid->setVerticalSpacing(12);
    controlsGrid->setColumnStretch(1, 1);
    controlsGrid->setColumnStretch(3, 1);

    auto labelStyle = QStringLiteral("font: 11.5pt 'Microsoft YaHei UI'; color: %1;")
                          .arg(AppTheme::secondaryTextColor());

    m_exposureSpin = new NoWheelSpinBox(controlsCard);
    m_exposureSpin->setRange(1, 200);
    QSettings settings;
    m_exposureSpin->setValue(settings.value(QStringLiteral("camera/exposureMs"), 12).toInt());
    m_exposureSpin->setSuffix(QStringLiteral(" ms"));
    configureNumericInput(m_exposureSpin, 220);

    m_gainSpin = new NoWheelSpinBox(controlsCard);
    m_gainSpin->setRange(1, 20);
    m_gainSpin->setValue(settings.value(QStringLiteral("camera/gain"), 1).toInt());
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
    connect(m_exposureSpin, &QSpinBox::valueChanged, this, [this](int value) {
        QSettings().setValue(QStringLiteral("camera/exposureMs"), value);
        if (m_deviceState == DeviceWorker::State::Connected || m_deviceState == DeviceWorker::State::Monitoring) {
            requestCurrentParameters();
        }
    });
    connect(m_gainSpin, &QSpinBox::valueChanged, this, [this](int value) {
        QSettings().setValue(QStringLiteral("camera/gain"), value);
        if (m_deviceState == DeviceWorker::State::Connected || m_deviceState == DeviceWorker::State::Monitoring) {
            requestCurrentParameters();
        }
    });

    auto *previewCard = new QWidget(page);
    previewCard->setStyleSheet(AppTheme::sectionCardStyle());
    auto *previewLayout = new QVBoxLayout(previewCard);
    previewLayout->setContentsMargins(24, 22, 24, 22);
    previewLayout->setSpacing(12);

    auto *previewTitle = new QLabel(QStringLiteral("实时画面区"), previewCard);
    previewTitle->setStyleSheet(subtleSectionTitleStyle());
    previewLayout->addWidget(previewTitle);

    auto *previewStack = new QWidget(previewCard);
    previewStack->setFixedHeight(520);
    auto *stackLayout = new QStackedLayout(previewStack);
    stackLayout->setContentsMargins(0, 0, 0, 0);
    stackLayout->setStackingMode(QStackedLayout::StackAll);

    m_cameraView = new QLabel(previewStack);
    m_cameraView->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_cameraView->setAlignment(Qt::AlignCenter);
    m_cameraView->setStyleSheet(QStringLiteral(
        "QLabel {"
            " background: #f8fafc;"
            " border: 1px solid #dbe3ec;"
            " border-radius: 8px;"
            " color: #64748b;"
            " font: 12pt 'Microsoft YaHei UI';"
            " }"));
    m_cameraView->setText(QStringLiteral("暂未获取实时图像。"));
    stackLayout->addWidget(m_cameraView);

    m_cameraLoadingOverlay = new QWidget(previewStack);
    m_cameraLoadingOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_cameraLoadingOverlay->setStyleSheet(QStringLiteral(
        "QWidget { background: rgba(248, 250, 252, 242); border: 1px solid #dbe3ec; border-radius: 8px; }"));
    auto *loadingLayout = new QVBoxLayout(m_cameraLoadingOverlay);
    loadingLayout->setContentsMargins(20, 20, 20, 20);
    loadingLayout->setSpacing(12);
    loadingLayout->addStretch();
    auto *loadingRing = new BusyRingWidget(m_cameraLoadingOverlay);
    loadingRing->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *ringRow = new QHBoxLayout();
    ringRow->addStretch();
    ringRow->addWidget(loadingRing);
    ringRow->addStretch();
    loadingLayout->addLayout(ringRow);
    m_cameraLoadingText = new QLabel(QStringLiteral("正在建立相机图像流…"), m_cameraLoadingOverlay);
    m_cameraLoadingText->setAlignment(Qt::AlignCenter);
    m_cameraLoadingText->setStyleSheet(QStringLiteral(
        "QLabel { color: #0f766e; font: 700 11pt 'Microsoft YaHei UI'; border: none; background: transparent; }"));
    loadingLayout->addWidget(m_cameraLoadingText);
    loadingLayout->addStretch();
    stackLayout->addWidget(m_cameraLoadingOverlay);
    m_cameraLoadingOverlay->hide();

    previewLayout->addWidget(previewStack, 1);

    layout->addWidget(controlsCard);
    layout->addWidget(previewCard, 1);
    return page;
}

void MainWindow::appendLog(const QString &message)
{
    FileLogger::write(message);
    m_logOutput->appendPlainText(QStringLiteral("[%1] %2")
                                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), message));
}

void MainWindow::setupDeviceWorkers()
{
    qRegisterMetaType<DeviceWorker::State>("DeviceWorker::State");
    if (hasSpectrometer()) {
        setupDeviceWorker(true);
    }
    if (hasCamera()) {
        setupDeviceWorker(false);
    }
}

void MainWindow::setupDeviceWorker(bool spectrometer)
{
    auto *thread = new QThread(this);
    const DeviceProfile emptyProfile;
    auto *worker = spectrometer ? new DeviceWorker(m_spectrometerProfile, emptyProfile)
                                : new DeviceWorker(emptyProfile, m_cameraProfile);
    worker->moveToThread(thread);

    if (spectrometer) {
        m_spectrometerThread = thread;
        m_spectrometerWorker = worker;
    } else {
        m_cameraThread = thread;
        m_cameraWorker = worker;
    }

    connect(thread, &QThread::started, worker, &DeviceWorker::initialize);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &MainWindow::connectDeviceRequested, worker, &DeviceWorker::connectDevice, Qt::QueuedConnection);
    connect(this, &MainWindow::disconnectDeviceRequested, worker, &DeviceWorker::disconnectDevice, Qt::QueuedConnection);
    connect(this, &MainWindow::startMonitoringRequested, worker, &DeviceWorker::startMonitoring, Qt::QueuedConnection);
    connect(this, &MainWindow::stopMonitoringRequested, worker, &DeviceWorker::stopMonitoring, Qt::QueuedConnection);
    connect(this, &MainWindow::shutdownWorkerRequested, worker, &DeviceWorker::shutdown, Qt::QueuedConnection);
    if (spectrometer) {
        connect(this, &MainWindow::spectrometerParametersRequested,
                worker, &DeviceWorker::setSpectrometerParameters, Qt::QueuedConnection);
        connect(worker, &DeviceWorker::spectrumReady, this, &MainWindow::handleSpectrum);
    } else {
        connect(this, &MainWindow::cameraParametersRequested,
                worker, &DeviceWorker::setCameraParameters, Qt::QueuedConnection);
        connect(worker, &DeviceWorker::frameReady, this, &MainWindow::handleCameraFrame);
    }
    connect(worker, &DeviceWorker::stateChanged, this, &MainWindow::handleWorkerState);
    connect(worker, &DeviceWorker::operationError, this, [this, spectrometer](const QString &message) {
        appendLog(QStringLiteral("%1错误：%2")
                      .arg(spectrometer ? QStringLiteral("光谱仪") : QStringLiteral("相机"), message));
    });
    thread->start();
}

void MainWindow::shutdownDeviceWorker(QThread *thread, DeviceWorker *worker)
{
    if (!thread || !worker || !thread->isRunning()) {
        return;
    }
    QMetaObject::invokeMethod(worker, "shutdown", Qt::QueuedConnection);
    thread->quit();
    if (!thread->wait(5000)) {
        FileLogger::write(QStringLiteral("设备线程未在 5 秒内退出，已转为后台清理。"), QStringLiteral("WARN"));
        thread->setParent(nullptr);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    }
}

void MainWindow::requestCurrentParameters()
{
    if (hasSpectrometer() && m_integrationSpin && m_smoothingSpin && m_averageSpin) {
        emit spectrometerParametersRequested(m_integrationSpin->value(),
                                             m_smoothingSpin->value(),
                                             m_averageSpin->value());
    }
    if (hasCamera() && m_exposureSpin && m_gainSpin) {
        emit cameraParametersRequested(m_exposureSpin->value(), m_gainSpin->value());
    }
}

bool MainWindow::validateJobContext()
{
    if (!m_sampleIdEdit || !m_sampleIdEdit->text().trimmed().isEmpty()) {
        return true;
    }
    QMessageBox::warning(this,
                         QStringLiteral("缺少样品编号"),
                         QStringLiteral("开始检测前请填写样品编号，确保采集数据可以追溯。"));
    m_sampleIdEdit->setFocus();
    return false;
}

QString MainWindow::currentSpectrometerDeviceId() const
{
    return m_spectrometerProfile.id;
}

QString MainWindow::currentCameraDeviceId() const
{
    return m_cameraProfile.id;
}

void MainWindow::updateOverview()
{
    if (isSpectrometerMode()) {
        m_headerLabel->setText(m_spectrometerProfile.name);
        m_deviceSummary->setText(QStringLiteral("检测设备：%1\n波段范围：%2")
                                     .arg(m_spectrometerProfile.name, m_spectrometerProfile.wavelengthRange));
        m_frameSummary->setText(QStringLiteral("不适用"));
        applySemanticCardStyle(m_frameCard, m_frameSummary, SummarySemantic::Disabled);
    } else if (isCameraMode()) {
        m_headerLabel->setText(m_cameraProfile.name);
        m_deviceSummary->setText(QStringLiteral("检测设备：%1").arg(m_cameraProfile.name));
        m_peakSummary->setText(QStringLiteral("不适用"));
        applySemanticCardStyle(m_peakCard, m_peakSummary, SummarySemantic::Disabled);
    } else if (isFusionMode()) {
        m_headerLabel->setText(QStringLiteral("光谱与图像融合检测"));
        m_deviceSummary->setText(QStringLiteral("检测设备：%1 + %2")
                                     .arg(m_spectrometerProfile.name, m_cameraProfile.name));
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
    const bool monitoring = m_deviceState == DeviceWorker::State::Monitoring ||
                            m_deviceState == DeviceWorker::State::Reconnecting;
    const bool busy = m_deviceState == DeviceWorker::State::Connecting ||
                      m_deviceState == DeviceWorker::State::Reconnecting;
    m_connectButton->setEnabled(!busy && !monitoring);
    m_connectButton->setText(connected ? QStringLiteral("断开设备") : QStringLiteral("连接设备"));
    m_connectButton->setIcon(AppTheme::tintedStandardIcon(
        style(), connected ? QStyle::SP_DialogCancelButton : QStyle::SP_DialogApplyButton));
    m_startButton->setEnabled(connected && !monitoring);
    m_stopButton->setEnabled(monitoring);
    if (m_sampleIdEdit) {
        m_sampleIdEdit->setEnabled(!monitoring);
        m_batchIdEdit->setEnabled(!monitoring);
        m_operatorEdit->setEnabled(!monitoring);
    }
}

void MainWindow::setSpectrumDisplayMode(SpectrumDisplayMode mode)
{
    if (m_spectrumDisplayMode == mode) {
        updateSpectrumModeUi();
        return;
    }

    m_spectrumDisplayMode = mode;
    updateSpectrumModeUi();
    appendLog(QStringLiteral("光谱模式：%1").arg(spectrumModeLabel()));
    if (!m_lastRawIntensities.isEmpty() && m_lastSpectrumTime.isValid()) {
        handleSpectrum(m_lastWavelengths, m_lastRawIntensities, m_lastSpectrumTime);
    }
}

void MainWindow::updateSpectrumModeUi()
{
    if (!m_rawModeButton || !m_reflectanceModeButton || !m_transmittanceModeButton ||
        !m_darkReferenceButton || !m_brightReferenceButton) {
        return;
    }

    m_rawModeButton->setChecked(m_spectrumDisplayMode == SpectrumDisplayMode::RawIntensity);
    m_reflectanceModeButton->setChecked(m_spectrumDisplayMode == SpectrumDisplayMode::Reflectance);
    m_transmittanceModeButton->setChecked(m_spectrumDisplayMode == SpectrumDisplayMode::Transmittance);

    const bool calibratedMode = m_spectrumDisplayMode != SpectrumDisplayMode::RawIntensity;
    m_darkReferenceButton->setEnabled(calibratedMode);
    m_brightReferenceButton->setEnabled(calibratedMode);
    if (m_spectrumDisplayMode == SpectrumDisplayMode::Transmittance) {
        m_brightReferenceButton->setText(QStringLiteral("采集透射参比"));
        m_brightReferenceButton->setToolTip(QStringLiteral("将无样品光路的当前光谱保存为透射参比 I0"));
    } else {
        m_brightReferenceButton->setText(QStringLiteral("采集白参考"));
        m_brightReferenceButton->setToolTip(QStringLiteral("将白色标准物的当前光谱保存为反射率白参考"));
    }
    m_darkReferenceButton->setToolTip(QStringLiteral("将当前实时光谱保存为当前模式的暗参考"));
}

QString MainWindow::spectrumModeId() const
{
    switch (m_spectrumDisplayMode) {
    case SpectrumDisplayMode::Reflectance:
        return QStringLiteral("reflectance");
    case SpectrumDisplayMode::Transmittance:
        return QStringLiteral("transmittance");
    case SpectrumDisplayMode::RawIntensity:
        return QStringLiteral("raw_intensity");
    }
    return QStringLiteral("raw_intensity");
}

QString MainWindow::spectrumModeLabel() const
{
    switch (m_spectrumDisplayMode) {
    case SpectrumDisplayMode::Reflectance:
        return QStringLiteral("反射率");
    case SpectrumDisplayMode::Transmittance:
        return QStringLiteral("透射率");
    case SpectrumDisplayMode::RawIntensity:
        return QStringLiteral("光照强度");
    }
    return QStringLiteral("光照强度");
}

void MainWindow::connectDevices()
{
    if (m_deviceState == DeviceWorker::State::Connected) {
        emit disconnectDeviceRequested();
        return;
    }
    requestCurrentParameters();
    emit connectDeviceRequested();
}

void MainWindow::startMonitoring()
{
    if (m_deviceState != DeviceWorker::State::Connected) {
        QMessageBox::warning(this, QStringLiteral("无法开始"), QStringLiteral("请先成功连接当前检测界面的设备。"));
        return;
    }
    if (!validateJobContext()) {
        return;
    }
    requestCurrentParameters();
    emit startMonitoringRequested();
}

void MainWindow::stopMonitoring()
{
    emit stopMonitoringRequested();
}

void MainWindow::handleWorkerState(DeviceWorker::State state, const QString &message)
{
    const bool fromSpectrometer = sender() == m_spectrometerWorker;
    if (fromSpectrometer) {
        m_spectrometerState = state;
        m_spectrometerStateMessage = message;
    } else {
        m_cameraState = state;
        m_cameraStateMessage = message;
    }
    if (!fromSpectrometer) {
        updateCameraWaitingState(state, message);
    }
    appendLog(QStringLiteral("%1：%2")
                  .arg(fromSpectrometer ? QStringLiteral("光谱仪") : QStringLiteral("相机"), message));
    updateCombinedWorkerState(message);
}

void MainWindow::updateCombinedWorkerState(const QString &message)
{
    if (isSpectrometerMode()) {
        m_deviceState = m_spectrometerState;
    } else if (isCameraMode()) {
        m_deviceState = m_cameraState;
    } else if (m_spectrometerState == DeviceWorker::State::Fault ||
               m_cameraState == DeviceWorker::State::Fault) {
        m_deviceState = DeviceWorker::State::Fault;
    } else if (m_spectrometerState == DeviceWorker::State::Reconnecting ||
               m_cameraState == DeviceWorker::State::Reconnecting) {
        m_deviceState = DeviceWorker::State::Reconnecting;
    } else if (m_spectrometerState == DeviceWorker::State::Connecting ||
               m_cameraState == DeviceWorker::State::Connecting) {
        m_deviceState = DeviceWorker::State::Connecting;
    } else if (m_spectrometerState == DeviceWorker::State::Monitoring &&
               m_cameraState == DeviceWorker::State::Monitoring) {
        m_deviceState = DeviceWorker::State::Monitoring;
    } else if (m_spectrometerState == DeviceWorker::State::Connected &&
               m_cameraState == DeviceWorker::State::Connected) {
        m_deviceState = DeviceWorker::State::Connected;
    } else if (m_spectrometerState == DeviceWorker::State::Disconnected &&
               m_cameraState == DeviceWorker::State::Disconnected) {
        m_deviceState = DeviceWorker::State::Disconnected;
    } else {
        m_deviceState = DeviceWorker::State::Connecting;
    }

    const bool connected = m_deviceState == DeviceWorker::State::Connected ||
                           m_deviceState == DeviceWorker::State::Monitoring;
    SummarySemantic semantic = SummarySemantic::Offline;
    switch (m_deviceState) {
    case DeviceWorker::State::Connecting:
    case DeviceWorker::State::Reconnecting:
        semantic = SummarySemantic::Warning;
        break;
    case DeviceWorker::State::Connected:
        semantic = SummarySemantic::Success;
        break;
    case DeviceWorker::State::Monitoring:
        semantic = SummarySemantic::Running;
        break;
    case DeviceWorker::State::Fault:
    case DeviceWorker::State::Disconnected:
        semantic = SummarySemantic::Offline;
        break;
    }
    const QString statusMessage = isFusionMode()
                                      ? QStringLiteral("光谱仪：%1 | 相机：%2")
                                            .arg(m_spectrometerStateMessage, m_cameraStateMessage)
                                      : message;
    m_statusSummary->setText(statusMessage);
    applySemanticCardStyle(m_statusCard, m_statusSummary, semantic);
    if (m_deviceState == DeviceWorker::State::Connected) {
        m_deviceSummary->setText(statusMessage);
    }
    statusBar()->showMessage(statusMessage);
    updateActionState(connected);

    if (m_autoStartMonitoring && m_deviceState == DeviceWorker::State::Connected) {
        m_autoStartMonitoring = false;
        emit startMonitoringRequested();
    }
}

void MainWindow::handleWorkerError(const QString &message)
{
    appendLog(QStringLiteral("设备错误：%1").arg(message));
}

void MainWindow::handleSpectrum(const QVector<double> &wavelengths,
                                const QVector<double> &intensities,
                                const QDateTime &capturedAt)
{
    m_lastWavelengths = wavelengths;
    m_lastRawIntensities = intensities;
    m_lastSpectrumTime = capturedAt;
    m_lastRefreshTime = capturedAt;
    refreshCalibratedSpectrum();

    int peakIndex = 0;
    double peakValue = m_lastDisplayIntensities.value(0);
    for (int i = 1; i < m_lastDisplayIntensities.size(); ++i) {
        if (m_lastDisplayIntensities.at(i) > peakValue) {
            peakValue = m_lastDisplayIntensities.at(i);
            peakIndex = i;
        }
    }

    QString chartTitle;
    QString valueLabel;
    QString peakLabel;
    QString curveColor;
    if (m_spectrumDisplayMode == SpectrumDisplayMode::Reflectance) {
        chartTitle = m_lastSpectrumCalibrated
                         ? QStringLiteral("实时反射率 - %1").arg(m_spectrometerProfile.name)
                         : QStringLiteral("实时光照强度（反射率参考未就绪） - %1").arg(m_spectrometerProfile.name);
        valueLabel = m_lastSpectrumCalibrated ? QStringLiteral("反射率") : QStringLiteral("光照强度");
        peakLabel = m_lastSpectrumCalibrated ? QStringLiteral("反射率") : QStringLiteral("光照强度");
        curveColor = m_lastSpectrumCalibrated ? QStringLiteral("#0f766e") : QStringLiteral("#2563eb");
    } else if (m_spectrumDisplayMode == SpectrumDisplayMode::Transmittance) {
        chartTitle = m_lastSpectrumCalibrated
                         ? QStringLiteral("实时透射率 - %1").arg(m_spectrometerProfile.name)
                         : QStringLiteral("实时光照强度（透射参比未就绪） - %1").arg(m_spectrometerProfile.name);
        valueLabel = m_lastSpectrumCalibrated ? QStringLiteral("透射率") : QStringLiteral("光照强度");
        peakLabel = m_lastSpectrumCalibrated ? QStringLiteral("透射率") : QStringLiteral("光照强度");
        curveColor = m_lastSpectrumCalibrated ? QStringLiteral("#7c3aed") : QStringLiteral("#2563eb");
    } else {
        chartTitle = QStringLiteral("实时光照强度 - %1").arg(m_spectrometerProfile.name);
        valueLabel = QStringLiteral("光照强度");
        peakLabel = QStringLiteral("光照强度");
        curveColor = QStringLiteral("#2563eb");
    }

    m_chartWidget->setSeries(m_lastWavelengths,
                             m_lastDisplayIntensities,
                             chartTitle,
                             QColor(curveColor),
                             valueLabel);
    m_peakSummary->setText(QStringLiteral("%1峰值：%2 nm / %3")
                               .arg(peakLabel)
                               .arg(m_lastWavelengths.value(peakIndex), 0, 'f', 1)
                               .arg(peakValue, 0, 'f', m_lastSpectrumCalibrated ? 4 : 1));
    applySemanticCardStyle(m_peakCard, m_peakSummary, SummarySemantic::Running);
    m_refreshSummary->setText(m_lastRefreshTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    applySemanticCardStyle(m_refreshCard, m_refreshSummary, SummarySemantic::Success);
}

void MainWindow::handleCameraFrame(const QImage &image, const QDateTime &capturedAt)
{
    m_lastImage = image;
    if (m_cameraLoadingOverlay) {
        m_cameraLoadingOverlay->hide();
    }
    ++m_cameraFrameCount;
    updateCameraPreview();
    m_frameSummary->setText(QStringLiteral("%1 帧").arg(m_cameraFrameCount));
    applySemanticCardStyle(m_frameCard, m_frameSummary, SummarySemantic::Running);
    m_lastCameraTime = capturedAt;
    m_lastRefreshTime = capturedAt;
    m_refreshSummary->setText(m_lastRefreshTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    applySemanticCardStyle(m_refreshCard, m_refreshSummary, SummarySemantic::Success);
}

void MainWindow::updateCameraPreview()
{
    if (!m_cameraView || m_lastImage.isNull()) {
        return;
    }

    const QSize previewSize = m_cameraView->contentsRect().size();
    if (previewSize.isEmpty()) {
        return;
    }
    m_cameraView->setPixmap(QPixmap::fromImage(m_lastImage).scaled(
        previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::updateCameraWaitingState(DeviceWorker::State state, const QString &message)
{
    if (!m_cameraLoadingOverlay || !m_cameraLoadingText) {
        return;
    }
    const bool waiting = state == DeviceWorker::State::Connecting ||
                         state == DeviceWorker::State::Reconnecting;
    if (waiting) {
        m_cameraLoadingText->setText(message.isEmpty()
                                         ? QStringLiteral("正在建立相机图像流…")
                                         : message);
        m_cameraLoadingOverlay->show();
        m_cameraLoadingOverlay->raise();
    } else {
        m_cameraLoadingOverlay->hide();
        if (m_lastImage.isNull() && m_cameraView) {
            m_cameraView->setText(state == DeviceWorker::State::Fault
                                      ? QStringLiteral("相机图像流未建立，请检查网卡和设备状态。")
                                      : QStringLiteral("暂未获取实时图像。"));
        }
    }
}


void MainWindow::refreshCalibratedSpectrum()
{
    m_lastSpectrumCalibrated = false;
    m_lastReflectance.clear();
    m_lastTransmittance.clear();
    m_lastDisplayIntensities = m_lastRawIntensities;
    if (!hasSpectrometer() || m_lastRawIntensities.isEmpty() ||
        m_spectrumDisplayMode == SpectrumDisplayMode::RawIntensity) {
        return;
    }

    const QString deviceId = currentSpectrometerDeviceId();
    const int integrationTimeUs = m_integrationSpin->value();
    const int smoothing = m_smoothingSpin->value();
    const int averageCount = m_averageSpin->value();
    QString errorMessage;
    if (SpectrumCalibration::referencesMatch(m_darkReference,
                                             m_whiteReference,
                                             m_lastWavelengths,
                                             deviceId,
                                             integrationTimeUs,
                                             smoothing,
                                             averageCount,
                                             &errorMessage)) {
        SpectrumCalibration::calculateReflectance(m_lastRawIntensities,
                                                  m_darkReference,
                                                  m_whiteReference,
                                                  m_lastReflectance,
                                                  &errorMessage);
    }

    errorMessage.clear();
    if (SpectrumCalibration::referencesMatch(m_transmittanceDarkReference,
                                             m_transmittanceReference,
                                             m_lastWavelengths,
                                             deviceId,
                                             integrationTimeUs,
                                             smoothing,
                                             averageCount,
                                             &errorMessage)) {
        SpectrumCalibration::calculateTransmittance(m_lastRawIntensities,
                                                    m_transmittanceDarkReference,
                                                    m_transmittanceReference,
                                                    m_lastTransmittance,
                                                    &errorMessage);
    }

    if (m_spectrumDisplayMode == SpectrumDisplayMode::Reflectance &&
        m_lastReflectance.size() == m_lastRawIntensities.size()) {
        m_lastDisplayIntensities = m_lastReflectance;
        m_lastSpectrumCalibrated = true;
    } else if (m_spectrumDisplayMode == SpectrumDisplayMode::Transmittance &&
               m_lastTransmittance.size() == m_lastRawIntensities.size()) {
        m_lastDisplayIntensities = m_lastTransmittance;
        m_lastSpectrumCalibrated = true;
    }
}

void MainWindow::captureDark()
{
    if (m_spectrumDisplayMode == SpectrumDisplayMode::RawIntensity) {
        return;
    }
    if (m_lastRawIntensities.isEmpty() || !m_lastSpectrumTime.isValid() ||
        m_lastSpectrumTime.msecsTo(QDateTime::currentDateTime()) > 10000) {
        QMessageBox::warning(this, QStringLiteral("暂无光谱"), QStringLiteral("请先获取实时光谱，再采集暗参考。"));
        return;
    }

    const SpectrumReference reference{m_lastWavelengths,
                                      m_lastRawIntensities,
                                      m_lastSpectrumTime,
                                      currentSpectrometerDeviceId(),
                                      m_integrationSpin->value(),
                                      m_smoothingSpin->value(),
                                      m_averageSpin->value()};
    if (m_spectrumDisplayMode == SpectrumDisplayMode::Reflectance) {
        m_darkReference = reference;
    } else {
        m_transmittanceDarkReference = reference;
    }
    appendLog(QStringLiteral("%1暗参考采集完成。").arg(spectrumModeLabel()));
    handleSpectrum(m_lastWavelengths, m_lastRawIntensities, m_lastSpectrumTime);
}

void MainWindow::captureWhite()
{
    if (m_spectrumDisplayMode == SpectrumDisplayMode::RawIntensity) {
        return;
    }
    if (m_lastRawIntensities.isEmpty() || !m_lastSpectrumTime.isValid() ||
        m_lastSpectrumTime.msecsTo(QDateTime::currentDateTime()) > 10000) {
        QMessageBox::warning(this, QStringLiteral("暂无光谱"), QStringLiteral("请先获取实时光谱，再采集亮参考。"));
        return;
    }

    const SpectrumReference reference{m_lastWavelengths,
                                      m_lastRawIntensities,
                                      m_lastSpectrumTime,
                                      currentSpectrometerDeviceId(),
                                      m_integrationSpin->value(),
                                      m_smoothingSpin->value(),
                                      m_averageSpin->value()};
    if (m_spectrumDisplayMode == SpectrumDisplayMode::Reflectance) {
        m_whiteReference = reference;
        appendLog(QStringLiteral("反射率白参考采集完成。"));
    } else {
        m_transmittanceReference = reference;
        appendLog(QStringLiteral("透射参比 I0 采集完成。"));
    }
    handleSpectrum(m_lastWavelengths, m_lastRawIntensities, m_lastSpectrumTime);
}

QString MainWindow::currentExportDirectory() const
{
    QSettings settings;
    const QString saved = settings.value(QStringLiteral("export/lastDirectory")).toString();
    if (!saved.isEmpty() && QDir(saved).exists()) {
        return saved;
    }
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::rememberExportDirectory(const QString &fileName)
{
    QSettings().setValue(QStringLiteral("export/lastDirectory"), QFileInfo(fileName).absolutePath());
}

void MainWindow::exportSpectrumCsv()
{
    if (m_lastWavelengths.isEmpty() || m_lastRawIntensities.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("暂无数据"), QStringLiteral("当前没有可导出的光谱数据。"));
        return;
    }
    if (!validateJobContext()) {
        return;
    }

    const QString defaultName = QStringLiteral("%1/%2_%3_%4光谱.csv")
                                    .arg(currentExportDirectory(),
                                         safeFilePart(m_sampleIdEdit->text()),
                                         m_lastSpectrumTime.toString(QStringLiteral("yyyyMMdd_HHmmss")),
                                         spectrumModeLabel());
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("导出光谱 CSV"),
        defaultName,
        QStringLiteral("CSV 文件 (*.csv)"));
    if (fileName.isEmpty()) {
        return;
    }

    QSaveFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("无法打开目标文件。"));
        return;
    }

    QTextStream stream(&file);
    stream << "# sample_id," << csvCell(m_sampleIdEdit->text().trimmed()) << '\n';
    stream << "# batch_id," << csvCell(m_batchIdEdit->text().trimmed()) << '\n';
    stream << "# operator," << csvCell(m_operatorEdit->text().trimmed()) << '\n';
    stream << "# device_id," << csvCell(currentSpectrometerDeviceId()) << '\n';
    stream << "# device_name," << csvCell(m_spectrometerProfile.name) << '\n';
    stream << "# sdk," << csvCell(m_spectrometerProfile.sdkName) << '\n';
    stream << "# captured_at," << csvCell(m_lastSpectrumTime.toString(Qt::ISODateWithMs)) << '\n';
    stream << "# integration_time_us," << m_integrationSpin->value() << '\n';
    stream << "# smoothing," << m_smoothingSpin->value() << '\n';
    stream << "# average_count," << m_averageSpin->value() << '\n';
    stream << "# display_mode," << spectrumModeId() << '\n';
    stream << "# calibrated," << (m_lastSpectrumCalibrated ? "true" : "false") << '\n';
    stream << "wavelength_nm,raw_intensity,reflectance_dark_reference,white_reference,reflectance,"
              "transmittance_dark_reference,transmission_reference,transmittance\n";
    for (int i = 0; i < m_lastWavelengths.size(); ++i) {
        stream << QString::number(m_lastWavelengths.at(i), 'f', 4) << ','
               << QString::number(m_lastRawIntensities.at(i), 'f', 4) << ',';
        if (m_darkReference.isValid() && i < m_darkReference.intensities.size()) {
            stream << QString::number(m_darkReference.intensities.at(i), 'f', 4);
        }
        stream << ',';
        if (m_whiteReference.isValid() && i < m_whiteReference.intensities.size()) {
            stream << QString::number(m_whiteReference.intensities.at(i), 'f', 4);
        }
        stream << ',';
        if (i < m_lastReflectance.size()) {
            stream << QString::number(m_lastReflectance.at(i), 'f', 8);
        }
        stream << ',';
        if (m_transmittanceDarkReference.isValid() && i < m_transmittanceDarkReference.intensities.size()) {
            stream << QString::number(m_transmittanceDarkReference.intensities.at(i), 'f', 4);
        }
        stream << ',';
        if (m_transmittanceReference.isValid() && i < m_transmittanceReference.intensities.size()) {
            stream << QString::number(m_transmittanceReference.intensities.at(i), 'f', 4);
        }
        stream << ',';
        if (i < m_lastTransmittance.size()) {
            stream << QString::number(m_lastTransmittance.at(i), 'f', 8);
        }
        stream << '\n';
    }
    stream.flush();
    if (stream.status() != QTextStream::Ok || !file.commit()) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), QStringLiteral("文件未能完整写入目标位置。"));
        return;
    }

    rememberExportDirectory(fileName);
    QJsonObject auditParameters;
    auditParameters.insert(QStringLiteral("integration_time_us"), m_integrationSpin->value());
    auditParameters.insert(QStringLiteral("smoothing"), m_smoothingSpin->value());
    auditParameters.insert(QStringLiteral("average_count"), m_averageSpin->value());
    auditParameters.insert(QStringLiteral("display_mode"), spectrumModeId());
    QString auditError;
    if (!AuditStore::recordExport(m_sampleIdEdit->text().trimmed(),
                                  m_batchIdEdit->text().trimmed(),
                                  m_operatorEdit->text().trimmed(),
                                  currentSpectrometerDeviceId(),
                                  m_spectrometerProfile.name,
                                  QStringLiteral("spectrum_csv"),
                                  m_lastSpectrumTime.toString(Qt::ISODateWithMs),
                                  QFileInfo(fileName).absoluteFilePath(),
                                  auditParameters,
                                  m_lastSpectrumCalibrated,
                                  &auditError)) {
        appendLog(QStringLiteral("光谱已导出，但审计记录写入失败：%1").arg(auditError));
    }
    appendLog(QStringLiteral("光谱 CSV 已导出到：%1").arg(fileName));
}

void MainWindow::exportCurrentImage()
{
    if (m_lastImage.isNull()) {
        QMessageBox::warning(this, QStringLiteral("暂无图像"), QStringLiteral("当前没有可导出的相机图像。"));
        return;
    }
    if (!validateJobContext()) {
        return;
    }

    const QString defaultName = QStringLiteral("%1/%2_%3_相机.png")
                                    .arg(currentExportDirectory(),
                                         safeFilePart(m_sampleIdEdit->text()),
                                         m_lastCameraTime.toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString fileName = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存当前图像"),
        defaultName,
        QStringLiteral("PNG 文件 (*.png);;JPEG 文件 (*.jpg *.jpeg)"));
    if (fileName.isEmpty()) {
        return;
    }

    const QByteArray format = QFileInfo(fileName).suffix().toLower() == QStringLiteral("jpg") ||
                                      QFileInfo(fileName).suffix().toLower() == QStringLiteral("jpeg")
                                  ? QByteArrayLiteral("jpg")
                                  : QByteArrayLiteral("png");
    QSaveFile imageFile(fileName);
    if (!imageFile.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), QStringLiteral("无法打开目标图像文件。"));
        return;
    }
    QImageWriter writer(&imageFile, format);
    if (!writer.write(m_lastImage) || !imageFile.commit()) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), writer.errorString());
        return;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("sample_id"), m_sampleIdEdit->text().trimmed());
    metadata.insert(QStringLiteral("batch_id"), m_batchIdEdit->text().trimmed());
    metadata.insert(QStringLiteral("operator"), m_operatorEdit->text().trimmed());
    metadata.insert(QStringLiteral("device_id"), currentCameraDeviceId());
    metadata.insert(QStringLiteral("device_name"), m_cameraProfile.name);
    metadata.insert(QStringLiteral("sdk"), m_cameraProfile.sdkName);
    metadata.insert(QStringLiteral("captured_at"), m_lastCameraTime.toString(Qt::ISODateWithMs));
    metadata.insert(QStringLiteral("exposure_ms"), m_exposureSpin->value());
    metadata.insert(QStringLiteral("gain"), m_gainSpin->value());
    QSaveFile metadataFile(fileName + QStringLiteral(".json"));
    if (metadataFile.open(QIODevice::WriteOnly)) {
        metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Indented));
        if (!metadataFile.commit()) {
            appendLog(QStringLiteral("图像已保存，但元数据文件写入失败。"));
        }
    }

    rememberExportDirectory(fileName);
    QString auditError;
    if (!AuditStore::recordExport(m_sampleIdEdit->text().trimmed(),
                                  m_batchIdEdit->text().trimmed(),
                                  m_operatorEdit->text().trimmed(),
                                  currentCameraDeviceId(),
                                  m_cameraProfile.name,
                                  QStringLiteral("camera_image"),
                                  m_lastCameraTime.toString(Qt::ISODateWithMs),
                                  QFileInfo(fileName).absoluteFilePath(),
                                  metadata,
                                  false,
                                  &auditError)) {
        appendLog(QStringLiteral("图像已保存，但审计记录写入失败：%1").arg(auditError));
    }
    appendLog(QStringLiteral("相机图像已保存到：%1").arg(fileName));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_closeConfirmed) {
        event->accept();
        return;
    }
    if (m_deviceState == DeviceWorker::State::Monitoring ||
        m_deviceState == DeviceWorker::State::Reconnecting) {
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("确认退出"),
            QStringLiteral("设备仍在采集或重连中，是否停止设备并关闭窗口？"));
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        emit stopMonitoringRequested();
    }
    event->accept();
}
