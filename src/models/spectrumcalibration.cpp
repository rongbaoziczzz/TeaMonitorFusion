#include "spectrumcalibration.h"

#include <QtMath>

namespace {
bool calculateNormalizedSpectrum(const QVector<double> &raw,
                                 const SpectrumReference &dark,
                                 const SpectrumReference &bright,
                                 const QString &brightReferenceName,
                                 QVector<double> &result,
                                 QString *errorMessage)
{
    if (raw.size() != dark.intensities.size() || raw.size() != bright.intensities.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("原始光谱与参考数据长度不一致。");
        }
        return false;
    }

    result.resize(raw.size());
    for (int i = 0; i < raw.size(); ++i) {
        const double denominator = bright.intensities.at(i) - dark.intensities.at(i);
        if (denominator <= 1e-9) {
            result.clear();
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1必须在全部波长点上高于暗参考，当前参考无法生成可信校准结果。")
                                    .arg(brightReferenceName);
            }
            return false;
        }
        result[i] = (raw.at(i) - dark.intensities.at(i)) / denominator;
    }
    return true;
}
}

bool SpectrumReference::isValid() const
{
    return capturedAt.isValid() && !deviceId.isEmpty() && !wavelengths.isEmpty() &&
           wavelengths.size() == intensities.size() && integrationTimeUs > 0 && averageCount > 0;
}

bool SpectrumCalibration::referencesMatch(const SpectrumReference &dark,
                                          const SpectrumReference &white,
                                          const QVector<double> &wavelengths,
                                          const QString &deviceId,
                                          int integrationTimeUs,
                                          int smoothing,
                                          int averageCount,
                                          QString *errorMessage)
{
    if (!dark.isValid() || !white.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前模式的暗参考或亮参考尚未有效采集。");
        }
        return false;
    }
    if (dark.deviceId != deviceId || white.deviceId != deviceId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参考数据与当前设备不一致。");
        }
        return false;
    }
    if (dark.integrationTimeUs != integrationTimeUs || white.integrationTimeUs != integrationTimeUs ||
        dark.smoothing != smoothing || white.smoothing != smoothing ||
        dark.averageCount != averageCount || white.averageCount != averageCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参考数据与当前采集参数不一致，请重新采集当前模式的参考。");
        }
        return false;
    }
    if (dark.wavelengths.size() != wavelengths.size() || white.wavelengths.size() != wavelengths.size()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("参考数据长度与实时光谱不一致。");
        }
        return false;
    }
    for (int i = 0; i < wavelengths.size(); ++i) {
        if (qAbs(dark.wavelengths.at(i) - wavelengths.at(i)) > 0.01 ||
            qAbs(white.wavelengths.at(i) - wavelengths.at(i)) > 0.01) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("参考数据的波长轴与实时光谱不一致。");
            }
            return false;
        }
    }
    return true;
}

bool SpectrumCalibration::calculateReflectance(const QVector<double> &raw,
                                               const SpectrumReference &dark,
                                               const SpectrumReference &white,
                                               QVector<double> &reflectance,
                                               QString *errorMessage)
{
    return calculateNormalizedSpectrum(raw,
                                       dark,
                                       white,
                                       QStringLiteral("白参考"),
                                       reflectance,
                                       errorMessage);
}

bool SpectrumCalibration::calculateTransmittance(const QVector<double> &raw,
                                                 const SpectrumReference &dark,
                                                 const SpectrumReference &incident,
                                                 QVector<double> &transmittance,
                                                 QString *errorMessage)
{
    return calculateNormalizedSpectrum(raw,
                                       dark,
                                       incident,
                                       QStringLiteral("透射参比"),
                                       transmittance,
                                       errorMessage);
}
