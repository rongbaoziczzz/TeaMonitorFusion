#include "ebuscameraservice.h"

#ifdef TEA_MONITOR_HAS_EBUS_SDK
#include <PvBuffer.h>
#include <PvBufferConverter.h>
#include <PvDevice.h>
#include <PvDeviceGEV.h>
#include <PvDeviceInfo.h>
#include <PvGenParameterArray.h>
#include <PvImage.h>
#include <PvInterface.h>
#include <PvPipeline.h>
#include <PvPixelType.h>
#include <PvStream.h>
#include <PvStreamGEV.h>
#include <PvSystem.h>
#endif

#include <QImage>

EBusCameraService::EBusCameraService() = default;

EBusCameraService::~EBusCameraService()
{
    close();
}

QString EBusCameraService::serviceName() const
{
    return QStringLiteral("eBUS 工业相机");
}

QString EBusCameraService::sdkName() const
{
    return QStringLiteral("Pleora eBUS SDK");
}

QString EBusCameraService::deviceSummary() const
{
    return m_deviceSummary;
}

bool EBusCameraService::open(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_EBUS_SDK
    if (m_isOpen) {
        return true;
    }

    static PvSystem system;
    system.Find();

    m_selectedDeviceInfo = nullptr;
    for (uint32_t i = 0; i < system.GetInterfaceCount(); ++i) {
        const PvInterface *iface = system.GetInterface(i);
        if (!iface) {
            continue;
        }

        for (uint32_t j = 0; j < iface->GetDeviceCount(); ++j) {
            const PvDeviceInfo *info = iface->GetDeviceInfo(j);
            if (info) {
                m_selectedDeviceInfo = info;
                break;
            }
        }

        if (m_selectedDeviceInfo) {
            break;
        }
    }

    if (!m_selectedDeviceInfo) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未检测到可用的 eBUS 工业相机。");
        }
        return false;
    }

    PvResult result;
    PvDevice *device = PvDevice::CreateAndConnect(static_cast<const PvDeviceInfo *>(m_selectedDeviceInfo), &result);
    if (!device || !result.IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("eBUS 相机连接失败：%1").arg(result.GetDescription().GetAscii());
        }
        return false;
    }

    m_device = device;
    m_isOpen = true;
    auto *params = device->GetParameters();
    if (params) {
        PvString vendor;
        PvString model;
        if (params->GetStringValue("DeviceVendorName", vendor).IsOK() &&
            params->GetStringValue("DeviceModelName", model).IsOK()) {
            m_modelName = QStringLiteral("%1 %2").arg(vendor.GetAscii(), model.GetAscii());
        }
    }
    if (m_modelName.isEmpty()) {
        m_modelName = QStringLiteral("eBUS 工业相机");
    }
    m_deviceSummary = QStringLiteral("已连接 %1。").arg(m_modelName);
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("本机只检测到 eBUS Player，未找到 eBUS 开发头文件（如 PvSystem.h），当前版本无法编译真实相机接入。");
    }
    return false;
#endif
}

void EBusCameraService::close()
{
#ifdef TEA_MONITOR_HAS_EBUS_SDK
    if (m_device) {
        PvDevice::Free(static_cast<PvDevice *>(m_device));
        m_device = nullptr;
    }
#endif
    m_isOpen = false;
    m_deviceSummary = QStringLiteral("eBUS 工业相机已断开。");
}

bool EBusCameraService::isOpen() const
{
    return m_isOpen;
}

void EBusCameraService::setExposureMs(int exposureMs)
{
    m_exposureMs = exposureMs;
}

void EBusCameraService::setGain(int gain)
{
    m_gain = gain;
}

QImage EBusCameraService::grabFrame()
{
#ifdef TEA_MONITOR_HAS_EBUS_SDK
    if (!m_isOpen || !m_device || !m_selectedDeviceInfo) {
        return {};
    }

    auto *device = static_cast<PvDevice *>(m_device);
    const auto *info = static_cast<const PvDeviceInfo *>(m_selectedDeviceInfo);
    PvResult result;
    PvStream *stream = PvStream::CreateAndOpen(info, &result);
    if (!stream || !result.IsOK()) {
        return {};
    }

    PvPipeline pipeline(stream);
    pipeline.SetBufferCount(4);
    if (!pipeline.Start().IsOK()) {
        PvStream::Free(stream);
        return {};
    }

    if (auto *gevDevice = dynamic_cast<PvDeviceGEV *>(device)) {
        if (auto *gevStream = dynamic_cast<PvStreamGEV *>(stream)) {
            gevDevice->NegotiatePacketSize();
            gevDevice->SetStreamDestination(gevStream->GetLocalIPAddress(), gevStream->GetLocalPort());
        }
    }

    if (auto *params = device->GetParameters()) {
        params->SetEnumValue("AcquisitionMode", "Continuous");
        params->SetEnumValue("TriggerMode", "Off");
        params->ExecuteCommand("AcquisitionStart");
    }
    device->StreamEnable();

    PvBuffer *buffer = nullptr;
    PvResult opResult;
    const PvResult grabResult = pipeline.RetrieveNextBuffer(&buffer, 1000, &opResult);

    if (auto *params = device->GetParameters()) {
        params->ExecuteCommand("AcquisitionStop");
    }
    device->StreamDisable();
    pipeline.Stop();
    PvStream::Free(stream);

    if (!grabResult.IsOK() || !opResult.IsOK() || !buffer) {
        return {};
    }

    PvImage *image = buffer->GetImage();
    if (!image) {
        return {};
    }

    const PvPixelType pixelType = image->GetPixelType();
    const uint32_t width = image->GetWidth();
    const uint32_t height = image->GetHeight();
    const uint8_t *data = image->GetDataPointer();

    if (pixelType == PvPixelMono8) {
        const uint32_t stride = width + image->GetPaddingX();
        return QImage(data, width, height, stride, QImage::Format_Grayscale8).copy();
    }

    if (pixelType == PvPixelBayerRG8 || pixelType == PvPixelBayerBG8 ||
        pixelType == PvPixelBayerGR8 || pixelType == PvPixelBayerGB8) {
        static PvBufferConverter converter;
        converter.SetBayerFilter(PvBayerFilterSimple);

        PvBuffer rgbBuffer;
        PvImage *rgbImage = rgbBuffer.GetImage();
        if (!rgbImage || !rgbImage->Alloc(width, height, PvPixelRGB8Packed).IsOK()) {
            return {};
        }
        if (!converter.Convert(buffer, &rgbBuffer).IsOK()) {
            return {};
        }

        const uint32_t stride = rgbImage->GetWidth() * 3 + rgbImage->GetPaddingX();
        return QImage(rgbImage->GetDataPointer(),
                      rgbImage->GetWidth(),
                      rgbImage->GetHeight(),
                      stride,
                      QImage::Format_RGB888)
            .copy();
    }
#endif
    return {};
}
