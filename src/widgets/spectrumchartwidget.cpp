#include "spectrumchartwidget.h"

#include <QPainter>
#include <QPainterPath>

SpectrumChartWidget::SpectrumChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(520);
    clear(QStringLiteral("暂未获取实时光谱。"));
}

void SpectrumChartWidget::setSeries(const QVector<double> &wavelengths,
                                    const QVector<double> &intensities,
                                    const QString &title,
                                    const QColor &curveColor)
{
    m_wavelengths = wavelengths;
    m_intensities = intensities;
    m_title = title;
    m_curveColor = curveColor;
    m_emptyMessage.clear();
    update();
}

void SpectrumChartWidget::clear(const QString &message)
{
    m_wavelengths.clear();
    m_intensities.clear();
    m_title = QStringLiteral("光谱视图");
    m_emptyMessage = message;
    update();
}

void SpectrumChartWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#f8fafc"));

    const QRect card = rect().adjusted(8, 8, -8, -8);
    painter.fillRect(card, Qt::white);
    painter.setPen(QPen(QColor("#dbe3ec"), 1));
    painter.drawRoundedRect(card, 16, 16);

    painter.setPen(QColor("#0f172a"));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 15, QFont::Bold));
    painter.drawText(card.adjusted(20, 16, -20, -16), Qt::AlignTop | Qt::AlignLeft, m_title);

    const QRect plot = card.adjusted(74, 64, -30, -62);
    painter.setPen(QPen(QColor("#e2e8f0"), 1));
    for (int i = 0; i <= 4; ++i) {
        const int y = plot.top() + i * plot.height() / 4;
        painter.drawLine(plot.left(), y, plot.right(), y);
    }
    for (int i = 0; i <= 5; ++i) {
        const int x = plot.left() + i * plot.width() / 5;
        painter.drawLine(x, plot.top(), x, plot.bottom());
    }

    painter.setPen(QPen(QColor("#0f172a"), 1));
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    painter.drawLine(plot.bottomLeft(), plot.topLeft());

    if (m_wavelengths.size() < 2 || m_intensities.size() < 2) {
        painter.setPen(QColor("#64748b"));
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 12));
        painter.drawText(plot, Qt::AlignCenter, m_emptyMessage);
        return;
    }

    double minX = m_wavelengths.first();
    double maxX = m_wavelengths.last();
    double minY = m_intensities.first();
    double maxY = m_intensities.first();

    for (double value : m_intensities) {
        minY = qMin(minY, value);
        maxY = qMax(maxY, value);
    }

    if (qFuzzyCompare(minX, maxX)) {
        maxX += 1.0;
    }
    if (qFuzzyCompare(minY, maxY)) {
        maxY += 1.0;
    }

    QPainterPath path;
    for (int i = 0; i < m_wavelengths.size(); ++i) {
        const double normX = (m_wavelengths.at(i) - minX) / (maxX - minX);
        const double normY = (m_intensities.at(i) - minY) / (maxY - minY);
        const QPointF point(plot.left() + normX * plot.width(), plot.bottom() - normY * plot.height());
        if (i == 0) {
            path.moveTo(point);
        } else {
            path.lineTo(point);
        }
    }

    painter.setPen(QPen(m_curveColor, 3.0));
    painter.drawPath(path);

    painter.setPen(QColor("#475569"));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    painter.drawText(card.adjusted(20, 0, -20, -16), Qt::AlignBottom | Qt::AlignLeft,
                     QStringLiteral("%1 nm - %2 nm").arg(minX, 0, 'f', 1).arg(maxX, 0, 'f', 1));
    painter.drawText(card.adjusted(20, 0, -20, -16), Qt::AlignBottom | Qt::AlignRight,
                     QStringLiteral("最小值 %1 / 最大值 %2").arg(minY, 0, 'f', 1).arg(maxY, 0, 'f', 1));
}
