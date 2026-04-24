#include "mockspectrometerservice.h"

#include <QtMath>

QString MockSpectrometerService::serviceName() const
{
    return QStringLiteral("光谱模拟器");
}

QString MockSpectrometerService::sdkName() const
{
    return QStringLiteral("内置模拟源");
}

QString MockSpectrometerService::deviceSummary() const
{
    return QStringLiteral("模拟光谱仪，输出演示曲线，适合培训和软件验证。");
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

void MockSpectrometerService::setIntegrationTimeUs(int integrationTimeUs)
{
    m_integrationTimeUs = integrationTimeUs;
}

void MockSpectrometerService::setSmoothing(int smoothing)
{
    m_smoothing = smoothing;
}

void MockSpectrometerService::setAverageCount(int averageCount)
{
    m_averageCount = averageCount;
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
        const double noise = 12.0 * qSin(i * 0.7 + m_frameIndex);

        wavelengths.append(wavelength);
        intensities.append((peakA + peakB + baseline + noise) * scale / qMax(1, m_averageCount));
    }

    ++m_frameIndex;
    Q_UNUSED(m_smoothing);
    return true;
}
