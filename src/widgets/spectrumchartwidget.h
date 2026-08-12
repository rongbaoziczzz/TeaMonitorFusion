#pragma once

#include <QVector>
#include <QWidget>

class SpectrumChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SpectrumChartWidget(QWidget *parent = nullptr);

    void setSeries(const QVector<double> &wavelengths,
                   const QVector<double> &intensities,
                   const QString &title,
                   const QColor &curveColor,
                   const QString &valueLabel);
    void clear(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QVector<double> m_wavelengths;
    QVector<double> m_intensities;
    QString m_title;
    QString m_valueLabel = QStringLiteral("纵坐标");
    QString m_emptyMessage;
    QColor m_curveColor = QColor("#2563eb");
    QPoint m_mousePosition;
    int m_hoverIndex = -1;

    int dataCount() const;
    QRect plotRect() const;
    int nearestIndexForPosition(const QPoint &position) const;
    static QString formatValue(double value);
};
