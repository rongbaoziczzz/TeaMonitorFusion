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
    return QStringLiteral("工业相机");
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
    int discoveredDeviceCount = 0;
    for (uint32_t i = 0; i < system.GetInterfaceCount(); ++i) {
        const PvInterface *iface = system.GetInterface(i);
        if (!iface) {
            continue;
        }

        for (uint32_t j = 0; j < iface->GetDeviceCount(); ++j) {
            const PvDeviceInfo *info = iface->GetDeviceInfo(j);
            if (info) {
                ++discoveredDeviceCount;
                if (!m_selectedDeviceInfo) {
                    m_selectedDeviceInfo = info;
                }
            }
        }
    }

    if (!m_selectedDeviceInfo) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未检测到可用的工业相机。");
        }
        return false;
    }
    if (discoveredDeviceCount > 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("检测到 %1 台工业相机，无法确定目标设备，请联系系统管理员。")
                                .arg(discoveredDeviceCount);
        }
        m_selectedDeviceInfo = nullptr;
        return false;
    }

    PvResult result;
    PvDevice *device = PvDevice::CreateAndConnect(static_cast<const PvDeviceInfo *>(m_selectedDeviceInfo), &result);
    if (!device || !result.IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("工业相机连接失败：%1").arg(result.GetDescription().GetAscii());
        }
        return false;
    }

    m_device = device;
    auto *params = device->GetParameters();
    QString serialNumber;
    if (params) {
        PvString vendor;
        PvString model;
        PvString serial;
        if (params->GetStringValue("DeviceVendorName", vendor).IsOK() &&
            params->GetStringValue("DeviceModelName", model).IsOK()) {
            m_modelName = QStringLiteral("%1 %2").arg(vendor.GetAscii(), model.GetAscii());
        }
        if (params->GetStringValue("DeviceSerialNumber", serial).IsOK()) {
            serialNumber = QString::fromLatin1(serial.GetAscii());
        }
    }
    if (m_modelName.isEmpty()) {
        m_modelName = QStringLiteral("工业相机");
    }
    if (!serialNumber.isEmpty()) {
        m_modelName += QStringLiteral("，序列号：%1").arg(serialNumber);
    }

    const auto *info = static_cast<const PvDeviceInfo *>(m_selectedDeviceInfo);
    PvStream *stream = PvStream::CreateAndOpen(info, &result);
    if (!stream || !result.IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机数据流打开失败：%1").arg(result.GetDescription().GetAscii());
        }
        close();
        return false;
    }
    m_stream = stream;

    if (auto *gevDevice = dynamic_cast<PvDeviceGEV *>(device)) {
        if (auto *gevStream = dynamic_cast<PvStreamGEV *>(stream)) {
            gevDevice->NegotiatePacketSize();
            gevDevice->SetStreamDestination(gevStream->GetLocalIPAddress(), gevStream->GetLocalPort());
        }
    }

    auto *pipeline = new PvPipeline(stream);
    pipeline->SetBufferCount(8);
    result = pipeline->Start();
    if (!result.IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机采集启动失败：%1").arg(result.GetDescription().GetAscii());
        }
        delete pipeline;
        m_pipeline = nullptr;
        close();
        return false;
    }
    m_pipeline = pipeline;

    m_isOpen = true;
    if (!applyCameraParameters(errorMessage)) {
        close();
        return false;
    }

    result = device->StreamEnable();
    if (!result.IsOK() || !params || !params->ExecuteCommand("AcquisitionStart").IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("工业相机无法进入连续采集状态。");
        }
        close();
        return false;
    }

    m_deviceSummary = QStringLiteral("已连接 %1。").arg(m_modelName);
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("工业相机支持组件不可用，请联系系统管理员。");
    }
    return false;
#endif
}

void EBusCameraService::close()
{
#ifdef TEA_MONITOR_HAS_EBUS_SDK
    auto *device = static_cast<PvDevice *>(m_device);
    if (device) {
        if (auto *params = device->GetParameters()) {
            params->ExecuteCommand("AcquisitionStop");
        }
        device->StreamDisable();
    }
    if (m_pipeline) {
        auto *pipeline = static_cast<PvPipeline *>(m_pipeline);
        pipeline->Stop();
        delete pipeline;
        m_pipeline = nullptr;
    }
    if (m_stream) {
        PvStream::Free(static_cast<PvStream *>(m_stream));
        m_stream = nullptr;
    }
    if (m_device) {
        PvDevice::Free(static_cast<PvDevice *>(m_device));
        m_device = nullptr;
    }
    m_selectedDeviceInfo = nullptr;
#endif
    m_isOpen = false;
    m_deviceSummary = QStringLiteral("工业相机已断开。");
}

bool EBusCameraService::isOpen() const
{
    return m_isOpen;
}

bool EBusCameraService::setExposureMs(int exposureMs, QString *errorMessage)
{
    m_exposureMs = exposureMs;
    return applyCameraParameters(errorMessage);
}

bool EBusCameraService::setGain(int gain, QString *errorMessage)
{
    m_gain = gain;
    return applyCameraParameters(errorMessage);
}

QImage EBusCameraService::grabFrame(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_EBUS_SDK
    if (!m_isOpen || !m_device || !m_pipeline) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("工业相机尚未连接。");
        }
        return {};
    }

    auto *pipeline = static_cast<PvPipeline *>(m_pipeline);
    PvBuffer *buffer = nullptr;
    PvResult opResult;
    const PvResult grabResult = pipeline->RetrieveNextBuffer(&buffer, 1000, &opResult);
    if (!grabResult.IsOK() || !opResult.IsOK() || !buffer) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机取帧失败：%1 / %2")
                                .arg(grabResult.GetDescription().GetAscii(), opResult.GetDescription().GetAscii());
        }
        return {};
    }

    QImage frame;
    PvImage *image = buffer->GetImage();
    if (image) {
        const PvPixelType pixelType = image->GetPixelType();
        const uint32_t width = image->GetWidth();
        const uint32_t height = image->GetHeight();
        const uint8_t *data = image->GetDataPointer();

        if (pixelType == PvPixelMono8) {
            const uint32_t stride = width + image->GetPaddingX();
            frame = QImage(data, width, height, stride, QImage::Format_Grayscale8).copy();
        } else if (pixelType == PvPixelBayerRG8 || pixelType == PvPixelBayerBG8 ||
                   pixelType == PvPixelBayerGR8 || pixelType == PvPixelBayerGB8) {
            static PvBufferConverter converter;
            converter.SetBayerFilter(PvBayerFilterSimple);

            PvBuffer rgbBuffer;
            PvImage *rgbImage = rgbBuffer.GetImage();
            if (rgbImage && rgbImage->Alloc(width, height, PvPixelRGB8Packed).IsOK() &&
                converter.Convert(buffer, &rgbBuffer).IsOK()) {
                const uint32_t stride = rgbImage->GetWidth() * 3 + rgbImage->GetPaddingX();
                frame = QImage(rgbImage->GetDataPointer(),
                               rgbImage->GetWidth(),
                               rgbImage->GetHeight(),
                               stride,
                               QImage::Format_RGB888)
                            .copy();
            }
        }
    }
    pipeline->ReleaseBuffer(buffer);

    if (frame.isNull() && errorMessage) {
        *errorMessage = QStringLiteral("相机返回了不支持的图像格式。");
    }
    return frame;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("工业相机支持组件不可用，请联系系统管理员。");
    }
#endif
    return {};
}

bool EBusCameraService::applyCameraParameters(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_EBUS_SDK
    if (!m_device || !m_isOpen) {
        return true;
    }

    auto *params = static_cast<PvDevice *>(m_device)->GetParameters();
    if (!params) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取工业相机参数。");
        }
        return false;
    }

    PvResult exposureResult = params->SetFloatValue("ExposureTime", static_cast<double>(m_exposureMs) * 1000.0);
    if (!exposureResult.IsOK()) {
        exposureResult = params->SetFloatValue("ExposureTimeAbs", static_cast<double>(m_exposureMs) * 1000.0);
    }
    if (!exposureResult.IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置相机曝光失败：%1").arg(exposureResult.GetDescription().GetAscii());
        }
        return false;
    }

    PvResult gainResult = params->SetFloatValue("Gain", static_cast<double>(m_gain));
    if (!gainResult.IsOK()) {
        gainResult = params->SetIntegerValue("Gain", m_gain);
    }
    if (!gainResult.IsOK()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("设置相机增益失败：%1").arg(gainResult.GetDescription().GetAscii());
        }
        return false;
    }

    params->SetEnumValue("AcquisitionMode", "Continuous");
    params->SetEnumValue("TriggerMode", "Off");
    return true;
#else
    Q_UNUSED(errorMessage);
    return true;
#endif
}
