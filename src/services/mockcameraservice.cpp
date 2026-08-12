#include "mockcameraservice.h"

#include <QDateTime>
#include <QPainter>
#include <QtMath>

QString MockCameraService::serviceName() const
{
    return QStringLiteral("离线图像数据源");
}

QString MockCameraService::sdkName() const
{
    return QStringLiteral("内置数据源");
}

QString MockCameraService::deviceSummary() const
{
    return QStringLiteral("离线图像数据源已就绪。");
}

bool MockCameraService::open(QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_isOpen = true;
    m_frameIndex = 0;
    return true;
}

void MockCameraService::close()
{
    m_isOpen = false;
}

bool MockCameraService::isOpen() const
{
    return m_isOpen;
}

bool MockCameraService::setExposureMs(int exposureMs, QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_exposureMs = exposureMs;
    return true;
}

bool MockCameraService::setGain(int gain, QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_gain = gain;
    return true;
}

QImage MockCameraService::grabFrame(QString *errorMessage)
{
    if (!m_isOpen) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("离线图像数据源尚未连接。");
        }
        return {};
    }

    QImage image(960, 540, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor("#f4f7fb"));

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.fillRect(QRect(24, 24, 912, 492), QColor("#ffffff"));
    painter.setPen(QPen(QColor("#d9e2ec"), 2));
    painter.drawRoundedRect(QRect(24, 24, 912, 492), 18, 18);

    const int offset = (m_frameIndex * 17) % 320;
    painter.fillRect(QRect(80 + offset, 150, 160, 160), QColor("#1d4ed8"));
    painter.fillRect(QRect(240 + offset / 2, 220, 220, 90), QColor("#16a34a"));
    painter.setBrush(QColor("#f59e0b"));
    painter.drawEllipse(QPointF(650, 230 + 30 * qSin(m_frameIndex / 6.0)), 60, 60);

    painter.setPen(QColor("#0f172a"));
    QFont titleFont(QStringLiteral("Microsoft YaHei UI"), 18, QFont::Bold);
    painter.setFont(titleFont);
    painter.drawText(QRect(60, 50, 520, 40), QStringLiteral("工业相机实时画面"));

    QFont bodyFont(QStringLiteral("Microsoft YaHei UI"), 10);
    painter.setFont(bodyFont);
    painter.setPen(QColor("#475569"));
    painter.drawText(QRect(60, 88, 720, 24),
                     QStringLiteral("曝光: %1 ms   增益: %2   帧号: %3")
                         .arg(m_exposureMs)
                         .arg(m_gain)
                         .arg(++m_frameIndex));
    painter.drawText(QRect(60, 112, 720, 24),
                     QStringLiteral("时间: %1")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")));

    painter.setPen(QPen(QColor("#94a3b8"), 1, Qt::DashLine));
    painter.drawLine(480, 70, 480, 470);
    painter.drawLine(80, 270, 880, 270);

    painter.setPen(QColor("#b91c1c"));
    painter.drawText(QRect(700, 460, 180, 24), Qt::AlignRight, QStringLiteral("离线数据"));

    return image;
}
