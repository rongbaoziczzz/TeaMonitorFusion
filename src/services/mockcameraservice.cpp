#include "mockcameraservice.h"

#include <QDateTime>
#include <QPainter>
#include <QtMath>

QString MockCameraService::serviceName() const
{
    return QStringLiteral("工业相机模拟器");
}

QString MockCameraService::sdkName() const
{
    return QStringLiteral("内置模拟源");
}

QString MockCameraService::deviceSummary() const
{
    return QStringLiteral("模拟工业相机，可用于演示、培训和界面联调。");
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

void MockCameraService::setExposureMs(int exposureMs)
{
    m_exposureMs = exposureMs;
}

void MockCameraService::setGain(int gain)
{
    m_gain = gain;
}

QImage MockCameraService::grabFrame()
{
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
    painter.drawText(QRect(700, 460, 180, 24), Qt::AlignRight, QStringLiteral("模拟检测叠层"));

    return image;
}
