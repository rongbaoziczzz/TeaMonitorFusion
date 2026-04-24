#include "launcherwindow.h"

#include "models/devicedefinitions.h"
#include "ui/apptheme.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSizePolicy>
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
        resize(780, 620);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(32, 28, 32, 28);
        layout->setSpacing(18);

        auto *titleLabel = new QLabel(title, this);
        titleLabel->setStyleSheet(QStringLiteral("font: 700 30px 'Microsoft YaHei UI'; color: #0f172a;"));
        layout->addWidget(titleLabel);

        auto *subtitleLabel = new QLabel(subtitle, this);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setStyleSheet(QStringLiteral("font: 13pt 'Microsoft YaHei UI'; color: #475569; line-height: 1.6;"));
        layout->addWidget(subtitleLabel);

        m_listWidget = new QListWidget(this);
        m_listWidget->setSpacing(10);
        m_listWidget->setAlternatingRowColors(false);
        m_listWidget->setStyleSheet(AppTheme::listWidgetStyle() + AppTheme::verticalScrollBarStyle());

        for (int i = 0; i < m_profiles.size(); ++i) {
            const auto &profile = m_profiles.at(i);
            QString text = QStringLiteral("%1. %2").arg(i + 1).arg(profile.name);
            if (!profile.wavelengthRange.isEmpty() && profile.wavelengthRange != QStringLiteral("-")) {
                text += QStringLiteral("\n波段范围：%1").arg(profile.wavelengthRange);
            }
            text += QStringLiteral("\n使用 SDK：%1").arg(profile.sdkName);
            text += QStringLiteral("\n说明：%1").arg(profile.description);

            auto *item = new QListWidgetItem(text, m_listWidget);
            item->setForeground(QColor(AppTheme::textColor()));
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setSizeHint(QSize(0, 112));
        }

        if (m_listWidget->count() > 0) {
            m_listWidget->setCurrentRow(0);
        }
        layout->addWidget(m_listWidget, 1);

        auto *buttonRow = new QHBoxLayout();
        buttonRow->setSpacing(12);
        buttonRow->addStretch();

        auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
        auto *confirmButton = new QPushButton(QStringLiteral("进入该检测界面"), this);
        cancelButton->setMinimumSize(150, 54);
        confirmButton->setMinimumSize(210, 54);
        cancelButton->setStyleSheet(AppTheme::secondaryButtonStyle());
        confirmButton->setStyleSheet(AppTheme::primaryButtonStyle(QStringLiteral("#0f766e")));

        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        connect(confirmButton, &QPushButton::clicked, this, [this]() {
            if (m_listWidget->currentRow() >= 0 && m_listWidget->currentRow() < m_profiles.size()) {
                m_selectedProfile = m_profiles.at(m_listWidget->currentRow());
                accept();
            }
        });
        connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this]() {
            if (m_listWidget->currentRow() >= 0 && m_listWidget->currentRow() < m_profiles.size()) {
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
        if (profile.id == QStringLiteral("ocean-direct") || profile.id == QStringLiteral("oceanhood")) {
            options.append(profile);
        }
    }
    return options;
}

QVector<DeviceProfile> cameraChoices()
{
    QVector<DeviceProfile> options;
    for (const auto &profile : availableCameras()) {
        if (profile.id == QStringLiteral("ebus-camera") || profile.id == QStringLiteral("sim-camera")) {
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
    resize(1240, 860);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(46, 36, 46, 36);
    layout->setSpacing(26);

    auto *title = new QLabel(QStringLiteral("茶叶监测系统"), central);
    title->setStyleSheet(AppTheme::titleStyle(38));
    layout->addWidget(title);

    auto *subtitle = new QLabel(
        QStringLiteral("请选择检测模块。光谱仪检测与工业相机检测会分别进入独立子界面，每个子界面只保留当前设备需要的参数区、状态区与实时显示区。"),
        central);
    subtitle->setWordWrap(true);
    subtitle->setStyleSheet(QStringLiteral("font: 14pt 'Microsoft YaHei UI'; color: %1; line-height: 1.7;")
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
            " border-radius: 22px;"
            " }"
            "QLabel { border: none; background: transparent; color: %3; }"
            "QPushButton { border: none; }")
                                .arg(AppTheme::cardColor(), AppTheme::cardBorderColor(), AppTheme::textColor()));

        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(26, 24, 26, 24);
        cardLayout->setSpacing(14);

        auto *tagLabel = new QLabel(tag, card);
        tagLabel->setStyleSheet(AppTheme::tagStyle());
        tagLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        cardLayout->addWidget(tagLabel);

        auto *headingLabel = new QLabel(titleText, card);
        headingLabel->setStyleSheet(AppTheme::titleStyle(26));
        cardLayout->addWidget(headingLabel);

        auto *bodyLabel = new QLabel(body, card);
        bodyLabel->setWordWrap(true);
        bodyLabel->setStyleSheet(QStringLiteral("font: 12.5pt 'Microsoft YaHei UI'; color: %1; line-height: 1.7;")
                                     .arg(AppTheme::secondaryTextColor()));
        cardLayout->addWidget(bodyLabel, 1);

        auto *button = new QPushButton(buttonText, card);
        button->setMinimumHeight(54);
        button->setStyleSheet(AppTheme::primaryButtonStyle(buttonColor));
        cardLayout->addWidget(button);

        return qMakePair(card, button);
    };

    auto *moduleRow = new QHBoxLayout();
    moduleRow->setSpacing(22);

    const auto spectrometerCard = makeCard(
        QStringLiteral("模块一"),
        QStringLiteral("光谱仪检测"),
        QStringLiteral("点击后会先弹出光谱仪选择框，再进入海洋光学光谱仪检测界面或如海广电光谱仪检测界面。两个检测界面彼此独立。"),
        QStringLiteral("选择光谱仪"),
        QStringLiteral("#0f766e"));

    const auto cameraCard = makeCard(
        QStringLiteral("模块二"),
        QStringLiteral("工业相机检测"),
        QStringLiteral("点击后会先弹出工业相机选择框，再进入独立工业相机检测界面。界面中重点保留参数设置、实时画面与图像导出。"),
        QStringLiteral("选择工业相机"),
        QStringLiteral("#2563eb"));

    moduleRow->addWidget(spectrometerCard.first, 1);
    moduleRow->addWidget(cameraCard.first, 1);
    layout->addLayout(moduleRow);

    auto *planCard = new QWidget(central);
    planCard->setStyleSheet(AppTheme::noticeCardStyle());

    auto *planLayout = new QVBoxLayout(planCard);
    planLayout->setContentsMargins(26, 24, 26, 24);
    planLayout->setSpacing(12);

    auto *planTitle = new QLabel(QStringLiteral("当前操作路径"), planCard);
    planTitle->setStyleSheet(QStringLiteral("font: 700 22px 'Microsoft YaHei UI'; color: #1d4ed8;"));
    planLayout->addWidget(planTitle);

    m_summaryLabel = new QLabel(
        QStringLiteral("1. 点击“选择光谱仪”后，会弹出二级选择框：海洋光学光谱仪、如海广电光谱仪。\n"
                       "2. 点击“选择工业相机”后，会弹出工业相机选择框。\n"
                       "3. 进入子界面后，系统只保留当前设备对应的功能区和状态区。\n"
                       "4. 如需切换设备，可在子界面顶部返回首页重新选择。"),
        planCard);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet(QStringLiteral("font: 13pt 'Microsoft YaHei UI'; color: #1e3a8a; line-height: 1.8;"));
    planLayout->addWidget(m_summaryLabel);

    layout->addWidget(planCard);
    layout->addStretch();

    setCentralWidget(central);
    central->setStyleSheet(QStringLiteral("background: %1;").arg(AppTheme::surfaceColor()));

    connect(spectrometerCard.second, &QPushButton::clicked, this, &LauncherWindow::openSpectrometerDialog);
    connect(cameraCard.second, &QPushButton::clicked, this, &LauncherWindow::openCameraDialog);
}

void LauncherWindow::openSpectrometerDialog()
{
    DeviceChoiceDialog dialog(
        QStringLiteral("选择光谱仪"),
        QStringLiteral("请选择本次进入的光谱检测子界面。选择后将直接进入对应的独立检测窗口。"),
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
        QStringLiteral("请选择本次进入的工业相机检测子界面。选择后将直接进入对应的独立检测窗口。"),
        cameraChoices(),
        this);

    if (dialog.exec() == QDialog::Accepted) {
        emit openCameraRequested(dialog.selectedProfile());
    }
}
