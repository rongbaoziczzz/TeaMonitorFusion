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
                   const QColor &curveColor);
    void clear(const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> m_wavelengths;
    QVector<double> m_intensities;
    QString m_title;
    QString m_emptyMessage;
    QColor m_curveColor = QColor("#2563eb");
};
