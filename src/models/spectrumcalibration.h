#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct SpectrumReference
{
    QVector<double> wavelengths;
    QVector<double> intensities;
    QDateTime capturedAt;
    QString deviceId;
    int integrationTimeUs = 0;
    int smoothing = 0;
    int averageCount = 0;

    bool isValid() const;
};

class SpectrumCalibration
{
public:
    static bool referencesMatch(const SpectrumReference &dark,
                                const SpectrumReference &white,
                                const QVector<double> &wavelengths,
                                const QString &deviceId,
                                int integrationTimeUs,
                                int smoothing,
                                int averageCount,
                                QString *errorMessage = nullptr);

    static bool calculateReflectance(const QVector<double> &raw,
                                     const SpectrumReference &dark,
                                     const SpectrumReference &white,
                                     QVector<double> &reflectance,
                                     QString *errorMessage = nullptr);

    static bool calculateTransmittance(const QVector<double> &raw,
                                       const SpectrumReference &dark,
                                       const SpectrumReference &incident,
                                       QVector<double> &transmittance,
                                       QString *errorMessage = nullptr);
};
