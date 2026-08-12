#include "spectrumchartwidget.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

SpectrumChartWidget::SpectrumChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(520);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
    clear(QStringLiteral("暂未获取实时光谱。"));
}

void SpectrumChartWidget::setSeries(const QVector<double> &wavelengths,
                                    const QVector<double> &intensities,
                                    const QString &title,
                                    const QColor &curveColor,
                                    const QString &valueLabel)
{
    m_wavelengths = wavelengths;
    m_intensities = intensities;
    m_title = title;
    m_curveColor = curveColor;
    m_valueLabel = valueLabel;
    m_emptyMessage.clear();
    m_hoverIndex = nearestIndexForPosition(m_mousePosition);
    update();
}

void SpectrumChartWidget::clear(const QString &message)
{
    m_wavelengths.clear();
    m_intensities.clear();
    m_title = QStringLiteral("光谱视图");
    m_valueLabel = QStringLiteral("纵坐标");
    m_emptyMessage = message;
    m_hoverIndex = -1;
    update();
}

int SpectrumChartWidget::dataCount() const
{
    return qMin(m_wavelengths.size(), m_intensities.size());
}

QRect SpectrumChartWidget::plotRect() const
{
    return rect().adjusted(8, 8, -8, -8).adjusted(74, 64, -30, -62);
}

int SpectrumChartWidget::nearestIndexForPosition(const QPoint &position) const
{
    const QRect plot = plotRect();
    const int count = dataCount();
    if (count < 2 || !plot.contains(position)) {
        return -1;
    }

    const auto xBounds = std::minmax_element(m_wavelengths.cbegin(), m_wavelengths.cbegin() + count);
    const double minX = *xBounds.first;
    const double maxX = *xBounds.second;
    if (qFuzzyCompare(minX, maxX)) {
        return 0;
    }

    const double normalizedX = qBound(0.0,
                                      (position.x() - plot.left()) / static_cast<double>(plot.width()),
                                      1.0);
    const double targetWavelength = minX + normalizedX * (maxX - minX);
    int nearestIndex = 0;
    double nearestDistance = std::abs(m_wavelengths.first() - targetWavelength);
    for (int index = 1; index < count; ++index) {
        const double distance = std::abs(m_wavelengths.at(index) - targetWavelength);
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = index;
        }
    }
    return nearestIndex;
}

QString SpectrumChartWidget::formatValue(double value)
{
    const double magnitude = std::abs(value);
    if (magnitude < 1.0) {
        return QString::number(value, 'f', 6);
    }
    if (magnitude < 100.0) {
        return QString::number(value, 'f', 4);
    }
    return QString::number(value, 'f', 2);
}

void SpectrumChartWidget::mouseMoveEvent(QMouseEvent *event)
{
    m_mousePosition = event->position().toPoint();
    const int nextHoverIndex = nearestIndexForPosition(m_mousePosition);
    if (nextHoverIndex != m_hoverIndex) {
        m_hoverIndex = nextHoverIndex;
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void SpectrumChartWidget::leaveEvent(QEvent *event)
{
    m_hoverIndex = -1;
    update();
    QWidget::leaveEvent(event);
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

    const QRect plot = plotRect();
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

    painter.save();
    painter.setPen(QColor("#475569"));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    painter.translate(card.left() + 24, plot.center().y());
    painter.rotate(-90);
    painter.drawText(QRect(-plot.height() / 2, -12, plot.height(), 24), Qt::AlignCenter, m_valueLabel);
    painter.restore();

    const int count = dataCount();
    if (count < 2) {
        painter.setPen(QColor("#64748b"));
        painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 12));
        painter.drawText(plot, Qt::AlignCenter, m_emptyMessage);
        return;
    }

    const auto xBounds = std::minmax_element(m_wavelengths.cbegin(), m_wavelengths.cbegin() + count);
    const auto yBounds = std::minmax_element(m_intensities.cbegin(), m_intensities.cbegin() + count);
    double minX = *xBounds.first;
    double maxX = *xBounds.second;
    double minY = *yBounds.first;
    double maxY = *yBounds.second;

    if (qFuzzyCompare(minX, maxX)) {
        maxX += 1.0;
    }
    if (qFuzzyCompare(minY, maxY)) {
        maxY += 1.0;
    }

    QPainterPath path;
    for (int i = 0; i < count; ++i) {
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

    if (m_hoverIndex >= 0 && m_hoverIndex < count) {
        const double wavelength = m_wavelengths.at(m_hoverIndex);
        const double value = m_intensities.at(m_hoverIndex);
        const double normX = (wavelength - minX) / (maxX - minX);
        const double normY = (value - minY) / (maxY - minY);
        const QPointF dataPoint(plot.left() + normX * plot.width(),
                                plot.bottom() - normY * plot.height());

        painter.save();
        painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
        painter.setPen(QPen(QColor("#64748b"), 1, Qt::DashLine));
        painter.drawLine(QPointF(dataPoint.x(), plot.top()), QPointF(dataPoint.x(), plot.bottom()));
        painter.drawLine(QPointF(plot.left(), dataPoint.y()), QPointF(plot.right(), dataPoint.y()));
        painter.restore();

        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(m_curveColor);
        painter.drawEllipse(dataPoint, 5, 5);

        const QString wavelengthText = QStringLiteral("波长  %1 nm").arg(wavelength, 0, 'f', 3);
        const QString valueText = QStringLiteral("%1  %2").arg(m_valueLabel, formatValue(value));
        const QFont tooltipFont(QStringLiteral("Microsoft YaHei UI"), 9);
        const QFontMetrics metrics(tooltipFont);
        const int tooltipWidth = qMax(metrics.horizontalAdvance(wavelengthText),
                                      metrics.horizontalAdvance(valueText)) + 24;
        const int tooltipHeight = metrics.height() * 2 + 18;
        int tooltipX = qRound(dataPoint.x()) + 14;
        if (tooltipX + tooltipWidth > plot.right()) {
            tooltipX = qRound(dataPoint.x()) - tooltipWidth - 14;
        }
        int tooltipY = qRound(dataPoint.y()) - tooltipHeight - 12;
        if (tooltipY < plot.top()) {
            tooltipY = qRound(dataPoint.y()) + 12;
        }
        tooltipX = qBound(plot.left(), tooltipX, plot.right() - tooltipWidth);
        tooltipY = qBound(plot.top(), tooltipY, plot.bottom() - tooltipHeight);
        const QRect tooltipRect(tooltipX, tooltipY, tooltipWidth, tooltipHeight);

        painter.setPen(QPen(QColor("#cbd5e1"), 1));
        painter.setBrush(QColor(255, 255, 255, 246));
        painter.drawRoundedRect(tooltipRect, 5, 5);
        painter.setPen(QColor("#0f172a"));
        painter.setFont(tooltipFont);
        painter.drawText(tooltipRect.adjusted(12, 7, -12, -7),
                         Qt::AlignLeft | Qt::AlignTop,
                         wavelengthText + QLatin1Char('\n') + valueText);
    }

    painter.setPen(QColor("#475569"));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    painter.drawText(card.adjusted(20, 0, -20, -16), Qt::AlignBottom | Qt::AlignLeft,
                     QStringLiteral("%1 nm - %2 nm").arg(minX, 0, 'f', 1).arg(maxX, 0, 'f', 1));
    painter.drawText(card.adjusted(20, 0, -20, -16), Qt::AlignBottom | Qt::AlignRight,
                     QStringLiteral("最小值 %1 / 最大值 %2").arg(minY, 0, 'f', 1).arg(maxY, 0, 'f', 1));
}
