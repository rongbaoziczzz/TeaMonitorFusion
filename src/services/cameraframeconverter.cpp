#include "cameraframeconverter.h"

#include <algorithm>
#include <array>
#include <QByteArray>

namespace {

enum class BayerColor {
    Red,
    Green,
    Blue
};

BayerColor bayerColorAt(quint64 format, int x, int y)
{
    const bool evenX = (x & 1) == 0;
    const bool evenY = (y & 1) == 0;

    switch (format) {
    case CameraFrameConverter::BayerRG8:
        return evenY ? (evenX ? BayerColor::Red : BayerColor::Green)
                     : (evenX ? BayerColor::Green : BayerColor::Blue);
    case CameraFrameConverter::BayerBG8:
        return evenY ? (evenX ? BayerColor::Blue : BayerColor::Green)
                     : (evenX ? BayerColor::Green : BayerColor::Red);
    case CameraFrameConverter::BayerGR8:
        return evenY ? (evenX ? BayerColor::Green : BayerColor::Red)
                     : (evenX ? BayerColor::Blue : BayerColor::Green);
    case CameraFrameConverter::BayerGB8:
    default:
        return evenY ? (evenX ? BayerColor::Green : BayerColor::Blue)
                     : (evenX ? BayerColor::Red : BayerColor::Green);
    }
}

uchar interpolateBayerChannel(const uchar *data,
                              int width,
                              int height,
                              int rowStride,
                              quint64 format,
                              int x,
                              int y,
                              BayerColor channel)
{
    if (bayerColorAt(format, x, y) == channel) {
        return data[y * rowStride + x];
    }

    int sum = 0;
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        const int sampleY = y + dy;
        if (sampleY < 0 || sampleY >= height) {
            continue;
        }
        for (int dx = -1; dx <= 1; ++dx) {
            const int sampleX = x + dx;
            if (sampleX < 0 || sampleX >= width || (dx == 0 && dy == 0)) {
                continue;
            }
            if (bayerColorAt(format, sampleX, sampleY) == channel) {
                sum += data[sampleY * rowStride + sampleX];
                ++count;
            }
        }
    }
    return count > 0 ? static_cast<uchar>(sum / count) : data[y * rowStride + x];
}

int unpackedBitDepth(quint64 format)
{
    if (format == CameraFrameConverter::Mono10 ||
        format == CameraFrameConverter::BayerGR10 || format == CameraFrameConverter::BayerRG10 ||
        format == CameraFrameConverter::BayerGB10 || format == CameraFrameConverter::BayerBG10) {
        return 10;
    }
    if (format == CameraFrameConverter::Mono12 ||
        format == CameraFrameConverter::BayerGR12 || format == CameraFrameConverter::BayerRG12 ||
        format == CameraFrameConverter::BayerGB12 || format == CameraFrameConverter::BayerBG12) {
        return 12;
    }
    return 16;
}

bool isBayerUnpacked16(quint64 format)
{
    return format >= CameraFrameConverter::BayerGR10 && format <= CameraFrameConverter::BayerBG12;
}

bool hasRequiredData(qsizetype dataSize, int rowStride, int height, int bytesInLastRow)
{
    if (dataSize < 0 || rowStride <= 0 || height <= 0 || bytesInLastRow <= 0) {
        return false;
    }
    const qsizetype required = static_cast<qsizetype>(rowStride) * (height - 1) + bytesInLastRow;
    return required <= dataSize;
}

void autoStretch(QByteArray &pixels, qsizetype count)
{
    if (count < 256 || pixels.size() < count) {
        return;
    }

    std::array<int, 256> histogram{};
    for (qsizetype index = 0; index < count; ++index) {
        ++histogram[static_cast<uchar>(pixels.at(index))];
    }

    const int lowTarget = std::max(1, static_cast<int>(count / 100));
    const int highTarget = std::max(lowTarget + 1, static_cast<int>(count - count / 100));
    int low = 0;
    int high = 255;
    int cumulative = 0;
    for (int value = 0; value < 256; ++value) {
        cumulative += histogram[value];
        if (cumulative >= lowTarget) {
            low = value;
            break;
        }
    }
    cumulative = 0;
    for (int value = 0; value < 256; ++value) {
        cumulative += histogram[value];
        if (cumulative >= highTarget) {
            high = value;
            break;
        }
    }
    if (high - low < 2) {
        return;
    }

    const int scale = 255 * 1024 / (high - low);
    for (qsizetype index = 0; index < count; ++index) {
        const int value = static_cast<uchar>(pixels.at(index));
        const int stretched = std::clamp((value - low) * scale / 1024, 0, 255);
        pixels[index] = static_cast<char>(stretched);
    }
}

} // namespace

QImage CameraFrameConverter::fromPfnc(const void *data,
                                      qsizetype dataSize,
                                      int width,
                                      int height,
                                      int rowStride,
                                      quint64 pixelFormat)
{
    if (!data || width <= 0 || height <= 0) {
        return {};
    }

    const auto *bytes = static_cast<const uchar *>(data);
    if (pixelFormat == Mono8) {
        if (!hasRequiredData(dataSize, rowStride, height, width)) {
            return {};
        }
        QByteArray normalized(width * height, Qt::Uninitialized);
        for (int y = 0; y < height; ++y) {
            std::copy_n(reinterpret_cast<const char *>(bytes + y * rowStride),
                        width,
                        normalized.data() + y * width);
        }
        autoStretch(normalized, width * height);
        return QImage(reinterpret_cast<const uchar *>(normalized.constData()),
                      width, height, width, QImage::Format_Grayscale8).copy();
    }

    if (pixelFormat == Rgb8Packed || pixelFormat == Bgr8Packed) {
        if (!hasRequiredData(dataSize, rowStride, height, width * 3)) {
            return {};
        }
        const QImage::Format format = pixelFormat == Rgb8Packed ? QImage::Format_RGB888
                                                                : QImage::Format_BGR888;
        return QImage(bytes, width, height, rowStride, format).copy();
    }

    if (CameraFrameConverter::isUnpacked16(pixelFormat)) {
        if (!hasRequiredData(dataSize, rowStride, height, width * 2)) {
            return {};
        }

        const int bitDepth = unpackedBitDepth(pixelFormat);
        const int shift = std::max(0, bitDepth - 8);
        QByteArray normalized(width * height, Qt::Uninitialized);
        for (int y = 0; y < height; ++y) {
            const uchar *source = bytes + y * rowStride;
            uchar *target = reinterpret_cast<uchar *>(normalized.data()) + y * width;
            for (int x = 0; x < width; ++x) {
                const quint16 value = static_cast<quint16>(source[x * 2]) |
                                      (static_cast<quint16>(source[x * 2 + 1]) << 8);
                target[x] = static_cast<uchar>(std::min<quint16>(255u, value >> shift));
            }
        }
        autoStretch(normalized, width * height);

        if (!isBayerUnpacked16(pixelFormat)) {
            return QImage(reinterpret_cast<const uchar *>(normalized.constData()),
                          width,
                          height,
                          width,
                          QImage::Format_Grayscale8)
                .copy();
        }

        return fromPfnc(normalized.constData(),
                        normalized.size(),
                        width,
                        height,
                        width,
                        pixelFormat == BayerGR10 || pixelFormat == BayerGR12 ? BayerGR8
                        : pixelFormat == BayerRG10 || pixelFormat == BayerRG12 ? BayerRG8
                        : pixelFormat == BayerGB10 || pixelFormat == BayerGB12 ? BayerGB8
                                                                                : BayerBG8);
    }

    const bool isBayer = pixelFormat == BayerGR8 || pixelFormat == BayerRG8 ||
                         pixelFormat == BayerGB8 || pixelFormat == BayerBG8;
    if (!isBayer || !hasRequiredData(dataSize, rowStride, height, width)) {
        return {};
    }

    QByteArray normalized(width * height, Qt::Uninitialized);
    for (int y = 0; y < height; ++y) {
        std::copy_n(reinterpret_cast<const char *>(bytes + y * rowStride),
                    width,
                    normalized.data() + y * width);
    }
    autoStretch(normalized, width * height);
    const auto *bayerBytes = reinterpret_cast<const uchar *>(normalized.constData());

    QImage result(width, height, QImage::Format_RGB888);
    if (result.isNull()) {
        return {};
    }
    for (int y = 0; y < height; ++y) {
        uchar *target = result.scanLine(y);
        for (int x = 0; x < width; ++x) {
            target[x * 3] = interpolateBayerChannel(
                bayerBytes, width, height, width, pixelFormat, x, y, BayerColor::Red);
            target[x * 3 + 1] = interpolateBayerChannel(
                bayerBytes, width, height, width, pixelFormat, x, y, BayerColor::Green);
            target[x * 3 + 2] = interpolateBayerChannel(
                bayerBytes, width, height, width, pixelFormat, x, y, BayerColor::Blue);
        }
    }
    return result;
}
