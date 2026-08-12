#pragma once

#include <QImage>
#include <QtGlobal>

namespace CameraFrameConverter {

constexpr quint64 Mono8 = 0x01080001u;
constexpr quint64 Mono10 = 0x01100003u;
constexpr quint64 Mono12 = 0x01100005u;
constexpr quint64 Mono16 = 0x01100007u;
constexpr quint64 BayerGR8 = 0x01080008u;
constexpr quint64 BayerRG8 = 0x01080009u;
constexpr quint64 BayerGB8 = 0x0108000au;
constexpr quint64 BayerBG8 = 0x0108000bu;
constexpr quint64 BayerGR10 = 0x0110000cu;
constexpr quint64 BayerRG10 = 0x0110000du;
constexpr quint64 BayerGB10 = 0x0110000eu;
constexpr quint64 BayerBG10 = 0x0110000fu;
constexpr quint64 BayerGR12 = 0x01100010u;
constexpr quint64 BayerRG12 = 0x01100011u;
constexpr quint64 BayerGB12 = 0x01100012u;
constexpr quint64 BayerBG12 = 0x01100013u;
constexpr quint64 Rgb8Packed = 0x02180014u;
constexpr quint64 Bgr8Packed = 0x02180015u;

constexpr bool isUnpacked16(quint64 pixelFormat)
{
    return pixelFormat == Mono10 || pixelFormat == Mono12 || pixelFormat == Mono16 ||
           (pixelFormat >= BayerGR10 && pixelFormat <= BayerBG12);
}

QImage fromPfnc(const void *data,
                qsizetype dataSize,
                int width,
                int height,
                int rowStride,
                quint64 pixelFormat);

} // namespace CameraFrameConverter
