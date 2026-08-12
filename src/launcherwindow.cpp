#include "launcherwindow.h"

#include "models/devicedefinitions.h"
#include "services/auditstore.h"
#include "ui/apptheme.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace {

class DeviceChoiceDialog : public QDialog
{
public:
    DeviceChoiceDialog(const QString &title,
                       const QString &subtitle,
                       const QVector<DeviceProfile> &profiles,
                       QWidget *parent = nullptr)
        : QDialog(parent)
        , m_profiles(profiles)
    {
        setWindowTitle(title);
        setModal(true);
        resize(760, 560);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(24, 22, 24, 22);
        layout->setSpacing(14);

        auto *titleLabel = new QLabel(title, this);
        titleLabel->setStyleSheet(QStringLiteral("font: 700 24px 'Microsoft YaHei UI'; color: #0f172a;"));
        layout->addWidget(titleLabel);

        auto *subtitleLabel = new QLabel(subtitle, this);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setStyleSheet(QStringLiteral("font: 11pt 'Microsoft YaHei UI'; color: #475569;"));
        layout->addWidget(subtitleLabel);

        m_listWidget = new QListWidget(this);
        m_listWidget->setAccessibleName(QStringLiteral("设备选择列表"));
        m_listWidget->setSpacing(10);
        m_listWidget->setAlternatingRowColors(false);
        m_listWidget->setStyleSheet(AppTheme::listWidgetStyle() + AppTheme::verticalScrollBarStyle());

        for (int i = 0; i < m_profiles.size(); ++i) {
            const auto &profile = m_profiles.at(i);
            QString text = QStringLiteral("%1. %2").arg(i + 1).arg(profile.name);
            if (!profile.wavelengthRange.isEmpty() && profile.wavelengthRange != QStringLiteral("-")) {
                text += QStringLiteral("\n波段范围：%1").arg(profile.wavelengthRange);
            }
            if (profile.isAvailable && !profile.description.isEmpty()) {
                text += QStringLiteral("\n%1").arg(profile.description);
            }
            if (!profile.isAvailable) {
                text += QStringLiteral("\n状态：设备支持组件未安装");
            }

            auto *item = new QListWidgetItem(text, m_listWidget);
            item->setForeground(QColor(AppTheme::textColor()));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setSizeHint(QSize(0, 100));
            if (!profile.isAvailable) {
                item->setFlags(item->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsSelectable);
                item->setForeground(QColor(QStringLiteral("#94a3b8")));
            }
        }

        for (int row = 0; row < m_profiles.size(); ++row) {
            if (m_profiles.at(row).isAvailable) {
                m_listWidget->setCurrentRow(row);
                break;
            }
        }
        layout->addWidget(m_listWidget, 1);

        auto *buttonRow = new QHBoxLayout();
        buttonRow->setSpacing(12);
        buttonRow->addStretch();

        auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
        auto *confirmButton = new QPushButton(QStringLiteral("进入该检测界面"), this);
        cancelButton->setMinimumSize(120, 44);
        confirmButton->setMinimumSize(180, 44);
        cancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
        confirmButton->setIcon(AppTheme::tintedStandardIcon(style(), QStyle::SP_DialogApplyButton));
        confirmButton->setDefault(true);
        confirmButton->setAutoDefault(true);
        cancelButton->setAutoDefault(false);
        cancelButton->setStyleSheet(AppTheme::secondaryButtonStyle());
        confirmButton->setStyleSheet(AppTheme::primaryButtonStyle(QStringLiteral("#0f766e")));

        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        connect(confirmButton, &QPushButton::clicked, this, [this]() {
            if (m_listWidget->currentRow() >= 0 && m_listWidget->currentRow() < m_profiles.size() &&
                m_profiles.at(m_listWidget->currentRow()).isAvailable) {
                m_selectedProfile = m_profiles.at(m_listWidget->currentRow());
                accept();
            }
        });
        connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this]() {
            if (m_listWidget->currentRow() >= 0 && m_listWidget->currentRow() < m_profiles.size() &&
                m_profiles.at(m_listWidget->currentRow()).isAvailable) {
                m_selectedProfile = m_profiles.at(m_listWidget->currentRow());
                accept();
            }
        });

        buttonRow->addWidget(cancelButton);
        buttonRow->addWidget(confirmButton);
        layout->addLayout(buttonRow);

        setStyleSheet(QStringLiteral("QDialog { background: %1; }").arg(AppTheme::surfaceColor()));
    }

    DeviceProfile selectedProfile() const
    {
        return m_selectedProfile;
    }

private:
    QVector<DeviceProfile> m_profiles;
    DeviceProfile m_selectedProfile;
    QListWidget *m_listWidget = nullptr;
};

QVector<DeviceProfile> spectrometerChoices()
{
    QVector<DeviceProfile> options;
    for (const auto &profile : availableSpectrometers()) {
        if (profile.id == QStringLiteral("ocean-direct") || profile.id == QStringLiteral("oceanhood") ||
            profile.id == QStringLiteral("sim-spec")) {
            options.append(profile);
        }
    }
    return options;
}

QVector<DeviceProfile> cameraChoices()
{
    QVector<DeviceProfile> options;
    for (const auto &profile : availableCameras()) {
        if (profile.id == QStringLiteral("aravis-camera") || profile.id == QStringLiteral("sim-camera")) {
            options.append(profile);
        }
    }
    return options;
}

} // namespace

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("茶叶监测系统"));
    resize(1180, 820);
    setMinimumSize(920, 660);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(36, 28, 36, 24);
    layout->setSpacing(18);

    auto *headerRow = new QHBoxLayout();
    auto *brandIcon = new QLabel(central);
    brandIcon->setFixedSize(52, 52);
    brandIcon->setPixmap(QPixmap(QStringLiteral(":/icons/app_icon.png")).scaled(
        brandIcon->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    auto *brandText = new QVBoxLayout();
    brandText->setSpacing(2);
    auto *title = new QLabel(QStringLiteral("茶叶品质检测工作站"), central);
    title->setStyleSheet(AppTheme::titleStyle(30));
    auto *edition = new QLabel(QStringLiteral("光谱分析 · 工业视觉 · 数据追溯"), central);
    edition->setStyleSheet(QStringLiteral("font: 10.5pt 'Microsoft YaHei UI'; color: #64748b;"));
    brandText->addWidget(title);
    brandText->addWidget(edition);

    auto *historyButton = new QPushButton(QStringLiteral("检测记录"), central);
    historyButton->setMinimumSize(132, 44);
    historyButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    historyButton->setAccessibleName(QStringLiteral("检测记录"));
    historyButton->setToolTip(QStringLiteral("查看已导出的检测记录"));
    historyButton->setCursor(Qt::PointingHandCursor);
    historyButton->setStyleSheet(AppTheme::secondaryButtonStyle());
    headerRow->addWidget(brandIcon);
    headerRow->addLayout(brandText);
    headerRow->addStretch();
    headerRow->addWidget(historyButton);
    layout->addLayout(headerRow);

    auto *subtitle = new QLabel(
        QStringLiteral("选择本次检测方式"),
        central);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("font: 700 13pt 'Microsoft YaHei UI'; color: %1;")
                                .arg(AppTheme::secondaryTextColor()));
    layout->addWidget(subtitle);

    auto makeCard = [central](const QString &tag,
                              const QString &titleText,
                              const QString &body,
                              const QString &buttonText,
                              const QString &buttonColor) {
        auto *card = new QWidget(central);
        card->setStyleSheet(QStringLiteral(
            "QWidget {"
            " background: %1;"
            " border: 1px solid %2;"
            " border-radius: 8px;"
            " }"
            "QLabel { border: none; background: transparent; color: %3; }"
            "QPushButton { border: none; }")
                                .arg(AppTheme::cardColor(), AppTheme::cardBorderColor(), AppTheme::textColor()));

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(22, 20, 22, 20);
        cardLayout->setSpacing(12);

        auto *tagLabel = new QLabel(tag, card);
        tagLabel->setStyleSheet(AppTheme::tagStyle());
        tagLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        cardLayout->addWidget(tagLabel);

        auto *headingLabel = new QLabel(titleText, card);
        headingLabel->setStyleSheet(AppTheme::titleStyle(21));
        cardLayout->addWidget(headingLabel);

        auto *bodyLabel = new QLabel(body, card);
        bodyLabel->setWordWrap(true);
        bodyLabel->setStyleSheet(QStringLiteral("font: 10.8pt 'Microsoft YaHei UI'; color: %1;")
                                     .arg(AppTheme::secondaryTextColor()));
        cardLayout->addWidget(bodyLabel, 1);

        auto *button = new QPushButton(buttonText, card);
        button->setMinimumHeight(44);
        button->setIcon(AppTheme::tintedStandardIcon(card->style(), QStyle::SP_ArrowForward));
        button->setStyleSheet(AppTheme::primaryButtonStyle(buttonColor));
        cardLayout->addWidget(button);

        return qMakePair(card, button);
    };

    auto *moduleRow = new QHBoxLayout();
    moduleRow->setSpacing(22);

    const auto spectrometerCard = makeCard(
        QStringLiteral("模块一"),
        QStringLiteral("光谱仪检测"),
        QStringLiteral("可见光 / 近红外光谱检测"),
        QStringLiteral("选择光谱仪"),
        QStringLiteral("#0f766e"));

    const auto cameraCard = makeCard(
        QStringLiteral("模块二"),
        QStringLiteral("工业相机检测"),
        QStringLiteral("IMPERX B1621C · GigE Vision"),
        QStringLiteral("选择工业相机"),
        QStringLiteral("#2563eb"));

    moduleRow->addWidget(spectrometerCard.first, 1);
    moduleRow->addWidget(cameraCard.first, 1);
    layout->addLayout(moduleRow);

    const auto fusionCard = makeCard(
        QStringLiteral("融合模式"),
        QStringLiteral("光谱 + 图像同步检测"),
        QStringLiteral("光谱与图像联合检测"),
        QStringLiteral("进入融合检测"),
        QStringLiteral("#334155"));
    fusionCard.first->setMaximumHeight(190);
    layout->addWidget(fusionCard.first);
    layout->addStretch();

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(QStringLiteral("QScrollArea { border: none; background: %1; }")
                                  .arg(AppTheme::surfaceColor()) +
                              AppTheme::verticalScrollBarStyle());
    central->setMinimumWidth(920);
    scrollArea->setWidget(central);
    setCentralWidget(scrollArea);
    central->setStyleSheet(QStringLiteral("background: %1;").arg(AppTheme::surfaceColor()));
    statusBar()->showMessage(QStringLiteral("就绪 · 请选择检测方式"));

    connect(spectrometerCard.second, &QPushButton::clicked, this, &LauncherWindow::openSpectrometerDialog);
    connect(cameraCard.second, &QPushButton::clicked, this, &LauncherWindow::openCameraDialog);
    connect(fusionCard.second, &QPushButton::clicked, this, &LauncherWindow::openFusionDialogs);
    connect(historyButton, &QPushButton::clicked, this, &LauncherWindow::openHistoryDialog);
}

void LauncherWindow::openSpectrometerDialog()
{
    DeviceChoiceDialog dialog(
        QStringLiteral("选择光谱仪"),
        QStringLiteral("请选择本次检测使用的光谱仪。"),
        spectrometerChoices(),
        this);

    if (dialog.exec() == QDialog::Accepted) {
        emit openSpectrometerRequested(dialog.selectedProfile());
    }
}

void LauncherWindow::openCameraDialog()
{
    DeviceChoiceDialog dialog(
        QStringLiteral("选择工业相机"),
        QStringLiteral("请选择本次检测使用的工业相机。"),
        cameraChoices(),
        this);

    if (dialog.exec() == QDialog::Accepted) {
        emit openCameraRequested(dialog.selectedProfile());
    }
}

void LauncherWindow::openFusionDialogs()
{
    DeviceChoiceDialog spectrometerDialog(
        QStringLiteral("选择融合光谱仪"),
        QStringLiteral("第一步：选择本次融合检测使用的光谱仪。"),
        spectrometerChoices(),
        this);
    if (spectrometerDialog.exec() != QDialog::Accepted) {
        return;
    }

    DeviceChoiceDialog cameraDialog(
        QStringLiteral("选择融合工业相机"),
        QStringLiteral("第二步：选择本次融合检测使用的工业相机。"),
        cameraChoices(),
        this);
    if (cameraDialog.exec() != QDialog::Accepted) {
        return;
    }
    emit openFusionRequested(spectrometerDialog.selectedProfile(), cameraDialog.selectedProfile());
}

void LauncherWindow::openHistoryDialog()
{
    QString errorMessage;
    const QVector<QStringList> records = AuditStore::recentExports(300, &errorMessage);
    if (!errorMessage.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("读取失败"), errorMessage);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("检测导出记录"));
    dialog.resize(1280, 720);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    auto *recordCount = new QLabel(QStringLiteral("检测记录 · %1 条").arg(records.size()), &dialog);
    recordCount->setStyleSheet(AppTheme::titleStyle(22));
    layout->addWidget(recordCount);
    auto *table = new QTableWidget(records.size(), 9, &dialog);
    table->setHorizontalHeaderLabels({QStringLiteral("样品编号"), QStringLiteral("批次"),
                                      QStringLiteral("操作员"), QStringLiteral("设备"),
                                      QStringLiteral("数据类型"), QStringLiteral("采集时间"),
                                      QStringLiteral("导出时间"), QStringLiteral("文件路径"),
                                      QStringLiteral("已校准")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(38);
    for (int row = 0; row < records.size(); ++row) {
        for (int column = 0; column < records.at(row).size(); ++column) {
            table->setItem(row, column, new QTableWidgetItem(records.at(row).at(column)));
        }
    }
    table->setSortingEnabled(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Stretch);
    layout->addWidget(table);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"), &dialog);
    closeButton->setMinimumSize(120, 44);
    closeButton->setStyleSheet(AppTheme::secondaryButtonStyle());
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    dialog.exec();
}
