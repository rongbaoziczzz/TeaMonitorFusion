#include "mockspectrometerservice.h"

#include <QtMath>

QString MockSpectrometerService::serviceName() const
{
    return QStringLiteral("离线光谱数据源");
}

QString MockSpectrometerService::sdkName() const
{
    return QStringLiteral("内置数据源");
}

QString MockSpectrometerService::deviceSummary() const
{
    return QStringLiteral("离线光谱数据源已就绪。");
}

bool MockSpectrometerService::open(QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_isOpen = true;
    m_frameIndex = 0;
    return true;
}

void MockSpectrometerService::close()
{
    m_isOpen = false;
}

bool MockSpectrometerService::isOpen() const
{
    return m_isOpen;
}

bool MockSpectrometerService::setIntegrationTimeUs(int integrationTimeUs, QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_integrationTimeUs = integrationTimeUs;
    return true;
}

bool MockSpectrometerService::setSmoothing(int smoothing, QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_smoothing = smoothing;
    return true;
}

bool MockSpectrometerService::setAverageCount(int averageCount, QString *errorMessage)
{
    Q_UNUSED(errorMessage);
    m_averageCount = averageCount;
    return true;
}

bool MockSpectrometerService::acquire(QVector<double> &wavelengths,
                                      QVector<double> &intensities,
                                      QString *errorMessage)
{
    Q_UNUSED(errorMessage);

    if (!m_isOpen) {
        return false;
    }

    wavelengths.clear();
    intensities.clear();

    const int points = 320;
    const double drift = qSin(m_frameIndex / 12.0) * 8.0;
    const double scale = 1.0 + (m_integrationTimeUs / 300000.0);

    for (int i = 0; i < points; ++i) {
        const double wavelength = 420.0 + i * 1.2;
        const double peakA = 950.0 * qExp(-qPow((wavelength - 540.0 - drift) / 30.0, 2.0));
        const double peakB = 620.0 * qExp(-qPow((wavelength - 610.0 + drift * 0.5) / 22.0, 2.0));
        const double baseline = 90.0 + 30.0 * qSin(i / 14.0 + m_frameIndex / 5.0);
        const double noise = (12.0 / qSqrt(qMax(1, m_averageCount))) * qSin(i * 0.7 + m_frameIndex);

        wavelengths.append(wavelength);
        intensities.append((peakA + peakB + baseline + noise) * scale);
    }

    if (m_smoothing > 0) {
        const QVector<double> unsmoothed = intensities;
        for (int i = 0; i < intensities.size(); ++i) {
            const int first = qMax(0, i - m_smoothing);
            const int last = qMin(intensities.size() - 1, i + m_smoothing);
            double sum = 0.0;
            for (int j = first; j <= last; ++j) {
                sum += unsmoothed.at(j);
            }
            intensities[i] = sum / (last - first + 1);
        }
    }

    ++m_frameIndex;
    return true;
}
