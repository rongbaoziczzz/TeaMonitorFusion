#include "araviscameraservice.h"

#include "cameraframeconverter.h"

#ifdef TEA_MONITOR_HAS_ARAVIS
#ifdef signals
#undef signals
#endif
#include <arv.h>
#endif

#include <QByteArray>
#include <QStringList>

#include <algorithm>

#ifdef TEA_MONITOR_HAS_ARAVIS
namespace {

QString consumeError(const QString &operation, GError *error)
{
    const QString detail = error && error->message ? QString::fromUtf8(error->message)
                                                    : QStringLiteral("未知错误");
    if (error) {
        g_error_free(error);
    }
    return QStringLiteral("%1：%2").arg(operation, detail);
}

QString utf8OrFallback(const char *value, const QString &fallback)
{
    return value && *value ? QString::fromUtf8(value) : fallback;
}

} // namespace
#endif

AravisCameraService::AravisCameraService(const QString &expectedSerialNumber,
                                         const QString &preferredAddress,
                                         const QString &preferredInterfaceAddress)
    : m_expectedSerialNumber(expectedSerialNumber.trimmed())
    , m_preferredAddress(preferredAddress.trimmed())
    , m_preferredInterfaceAddress(preferredInterfaceAddress.trimmed())
{
}

AravisCameraService::~AravisCameraService()
{
    close();
}

QString AravisCameraService::serviceName() const
{
    return QStringLiteral("IMPERX GigE Vision 工业相机");
}

QString AravisCameraService::sdkName() const
{
    return QStringLiteral("Aravis (GigE Vision / GenICam)");
}

QString AravisCameraService::deviceSummary() const
{
    return m_deviceSummary;
}

bool AravisCameraService::open(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    if (m_isOpen) {
        return true;
    }

    arv_update_device_list();
    const unsigned int deviceCount = arv_get_n_devices();
    QByteArray selectedDeviceId;
    QString selectedVendor;
    QString selectedModel;
    QString selectedSerial;
    QString selectedDeviceAddress;
    QStringList discoveredDevices;

    for (unsigned int index = 0; index < deviceCount; ++index) {
        const QString serial = utf8OrFallback(arv_get_device_serial_nbr(index), QStringLiteral("未知序列号"));
        const QString vendor = utf8OrFallback(arv_get_device_vendor(index), QStringLiteral("未知厂商"));
        const QString model = utf8OrFallback(arv_get_device_model(index), QStringLiteral("未知型号"));
        const char *deviceId = arv_get_device_id(index);
        const QString deviceAddress = utf8OrFallback(arv_get_device_address(index), QString());
        discoveredDevices.append(deviceAddress.isEmpty()
                                     ? QStringLiteral("%1 %2 [%3]").arg(vendor, model, serial)
                                     : QStringLiteral("%1 %2 [%3] @ %4").arg(vendor, model, serial, deviceAddress));

        const bool serialMatches = m_expectedSerialNumber.isEmpty() ||
                                   serial.compare(m_expectedSerialNumber, Qt::CaseInsensitive) == 0;
        if (selectedDeviceId.isEmpty() && serialMatches && deviceId && *deviceId) {
            selectedDeviceId = QByteArray(deviceId);
            selectedVendor = vendor;
            selectedModel = model;
            selectedSerial = serial;
            selectedDeviceAddress = deviceAddress;
        }
    }

    if (m_expectedSerialNumber.isEmpty() && deviceCount > 1) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("发现多台相机，无法确定目标设备，请联系系统管理员：%1")
                                .arg(discoveredDevices.join(QStringLiteral("；")));
        }
        return false;
    }

    QStringList openAttempts;
    auto openOnInterface = [&openAttempts](const QString &interfaceAddress,
                                           const QString &deviceAddress) -> ArvCamera * {
        if (interfaceAddress.isEmpty() || deviceAddress.isEmpty()) {
            return nullptr;
        }

        GInetAddress *interfaceIp = g_inet_address_new_from_string(interfaceAddress.toLatin1().constData());
        GInetAddress *deviceIp = g_inet_address_new_from_string(deviceAddress.toLatin1().constData());
        if (!interfaceIp || !deviceIp) {
            openAttempts.append(QStringLiteral("固定网卡或相机地址格式无效：%1 → %2")
                                    .arg(interfaceAddress, deviceAddress));
            if (interfaceIp) {
                g_object_unref(interfaceIp);
            }
            if (deviceIp) {
                g_object_unref(deviceIp);
            }
            return nullptr;
        }

        GError *deviceError = nullptr;
        ArvDevice *device = arv_gv_device_new(interfaceIp, deviceIp, &deviceError);
        g_object_unref(interfaceIp);
        g_object_unref(deviceIp);
        if (!device || deviceError) {
            openAttempts.append(consumeError(
                QStringLiteral("通过固定网卡 %1 打开 %2 失败").arg(interfaceAddress, deviceAddress),
                deviceError));
            if (device) {
                g_object_unref(device);
            }
            return nullptr;
        }

        GError *cameraError = nullptr;
        ArvCamera *candidate = arv_camera_new_with_device(device, &cameraError);
        g_object_unref(device);
        if (!candidate || cameraError) {
            openAttempts.append(consumeError(QStringLiteral("创建固定网卡相机对象失败"), cameraError));
            if (candidate) {
                g_object_unref(candidate);
            }
            return nullptr;
        }
        return candidate;
    };
    auto openById = [&openAttempts](const QByteArray &deviceId, const QString &label) -> ArvCamera * {
        if (deviceId.isEmpty()) {
            return nullptr;
        }
        GError *openError = nullptr;
        auto *candidate = arv_camera_new(deviceId.constData(), &openError);
        if (!candidate || openError) {
            openAttempts.append(consumeError(label, openError));
            if (candidate) {
                g_object_unref(candidate);
            }
            return nullptr;
        }
        return candidate;
    };

    const QString targetAddress = selectedDeviceAddress.isEmpty() ? m_preferredAddress : selectedDeviceAddress;
    ArvCamera *camera = openOnInterface(m_preferredInterfaceAddress, targetAddress);
    if (!camera && m_preferredInterfaceAddress.isEmpty()) {
        camera = openById(selectedDeviceId, QStringLiteral("打开工业相机失败"));
    }
    if (!camera && m_preferredInterfaceAddress.isEmpty() && !m_preferredAddress.isEmpty() &&
        m_preferredAddress.toLatin1() != selectedDeviceId) {
        camera = openById(m_preferredAddress.toLatin1(),
                          QStringLiteral("按固定 IP %1 打开工业相机失败").arg(m_preferredAddress));
    }
    if (!camera) {
        if (errorMessage) {
            QString message;
            if (deviceCount == 0 && m_preferredAddress.isEmpty()) {
                message = QStringLiteral(
                    "未发现 GigE Vision 相机。请确认相机供电、网线、防火墙和相机网卡 IPv4 配置。");
            } else if (!m_expectedSerialNumber.isEmpty() && selectedDeviceId.isEmpty() &&
                       m_preferredAddress.isEmpty()) {
                message = QStringLiteral("发现 %1 台相机，但未找到序列号 %2。已发现：%3")
                              .arg(deviceCount)
                              .arg(m_expectedSerialNumber, discoveredDevices.join(QStringLiteral("；")));
            } else {
                message = QStringLiteral("无法建立 GigE Vision 控制连接");
            }
            if (!discoveredDevices.isEmpty()) {
                message += QStringLiteral("。已发现：%1").arg(discoveredDevices.join(QStringLiteral("；")));
            }
            if (!openAttempts.isEmpty()) {
                message += QStringLiteral("。连接尝试：%1").arg(openAttempts.join(QStringLiteral("；")));
            }
            if (!m_preferredAddress.isEmpty()) {
                message += QStringLiteral("。有效目标地址：%1").arg(targetAddress);
            }
            *errorMessage = message;
        }
        return false;
    }

    auto readCameraString = [camera](const char *(*getter)(ArvCamera *, GError **),
                                     const QString &fallback) {
        GError *infoError = nullptr;
        const char *value = getter(camera, &infoError);
        if (infoError) {
            g_error_free(infoError);
        }
        return utf8OrFallback(value, fallback);
    };
    selectedVendor = readCameraString(arv_camera_get_vendor_name, QStringLiteral("未知厂商"));
    selectedModel = readCameraString(arv_camera_get_model_name, QStringLiteral("未知型号"));
    selectedSerial = readCameraString(arv_camera_get_device_serial_number, QStringLiteral("未知序列号"));
    if (!m_expectedSerialNumber.isEmpty() && selectedSerial != QStringLiteral("未知序列号") &&
        selectedSerial.compare(m_expectedSerialNumber, Qt::CaseInsensitive) != 0) {
        g_object_unref(camera);
        if (errorMessage) {
            *errorMessage = QStringLiteral("固定地址 %1 对应序列号 %2，与目标序列号 %3 不一致。")
                                .arg(targetAddress, selectedSerial, m_expectedSerialNumber);
        }
        return false;
    }
    m_camera = camera;
    m_isOpen = true;

    if (arv_camera_is_gv_device(camera)) {
        arv_camera_gv_set_packet_size_adjustment(
            camera, ARV_GV_PACKET_SIZE_ADJUSTMENT_ON_FAILURE_ONCE);

        // Keep the stream within a standard Ethernet MTU and shape the camera's
        // Gigabit burst below the sustained receive rate of the 100 Mbps adapter.
        GError *transportError = nullptr;
        arv_camera_gv_set_packet_size(camera, 1400, &transportError);
        if (transportError) {
            g_error_free(transportError);
            transportError = nullptr;
        }
        arv_camera_gv_set_packet_delay(camera, 130000, &transportError);
        if (transportError) {
            g_error_free(transportError);
        }
    }

    if (!applyCameraParameters(errorMessage)) {
        close();
        return false;
    }

    GError *frameRateError = nullptr;
    const bool frameRateAvailable = arv_camera_is_frame_rate_available(camera, &frameRateError);
    if (!frameRateError && frameRateAvailable) {
        arv_camera_set_frame_rate(camera, 2.0, &frameRateError);
    }
    if (frameRateError) {
        g_error_free(frameRateError);
    }

    if (!startStream(errorMessage)) {
        close();
        return false;
    }

    m_deviceSummary = QStringLiteral("已连接 %1 %2，序列号：%3%4")
                          .arg(selectedVendor,
                               selectedModel,
                               selectedSerial,
                               targetAddress.isEmpty()
                                   ? QString()
                                   : QStringLiteral("，地址：%1，本机网卡：%2")
                                         .arg(targetAddress,
                                              m_preferredInterfaceAddress.isEmpty()
                                                  ? QStringLiteral("自动选择")
                                                  : m_preferredInterfaceAddress));
    return true;
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("工业相机支持组件不可用，请联系系统管理员。");
    }
    return false;
#endif
}

bool AravisCameraService::startStream(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    auto *camera = static_cast<ArvCamera *>(m_camera);
    if (!camera || !m_isOpen) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GigE Vision 控制连接尚未建立。");
        }
        return false;
    }

    GError *error = nullptr;
    ArvStream *stream = arv_camera_create_stream(camera, nullptr, nullptr, &error);
    if (!stream || error) {
        if (errorMessage) {
            *errorMessage = consumeError(QStringLiteral("创建相机图像流失败"), error);
        } else if (error) {
            g_error_free(error);
        }
        if (stream) {
            g_object_unref(stream);
        }
        return false;
    }

    const unsigned int payload = arv_camera_get_payload(camera, &error);
    if (error || payload == 0) {
        if (errorMessage) {
            *errorMessage = consumeError(QStringLiteral("读取相机图像负载大小失败"), error);
        } else if (error) {
            g_error_free(error);
        }
        g_object_unref(stream);
        return false;
    }

    if (ARV_IS_GV_STREAM(stream)) {
        const guint64 requestedBufferSize = std::min<guint64>(
            static_cast<guint64>(payload) * 4u,
            static_cast<guint64>(32u * 1024u * 1024u));
        g_object_set(stream,
                     "socket-buffer", ARV_GV_STREAM_SOCKET_BUFFER_FIXED,
                     "socket-buffer-size", static_cast<int>(requestedBufferSize),
                     "packet-resend", ARV_GV_STREAM_PACKET_RESEND_ALWAYS,
                     "frame-retention", 250000u,
                     nullptr);
    }
    for (int index = 0; index < 12; ++index) {
        arv_stream_push_buffer(stream, arv_buffer_new_allocate(payload));
    }

    arv_camera_start_acquisition(camera, &error);
    if (error) {
        if (errorMessage) {
            *errorMessage = consumeError(QStringLiteral("启动相机连续采集失败"), error);
        } else {
            g_error_free(error);
        }
        g_object_unref(stream);
        return false;
    }

    m_stream = stream;
    return true;
#else
    Q_UNUSED(errorMessage);
    return false;
#endif
}

void AravisCameraService::stopStream()
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    auto *camera = static_cast<ArvCamera *>(m_camera);
    if (camera && m_isOpen) {
        GError *error = nullptr;
        arv_camera_stop_acquisition(camera, &error);
        if (error) {
            g_error_free(error);
        }
    }
    if (m_stream) {
        g_object_unref(m_stream);
        m_stream = nullptr;
    }
#endif
}

void AravisCameraService::close()
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    stopStream();
    if (m_camera) {
        g_object_unref(m_camera);
        m_camera = nullptr;
    }
#endif
    m_isOpen = false;
    m_deviceSummary = QStringLiteral("IMPERX GigE Vision 相机已断开。");
}

bool AravisCameraService::isOpen() const
{
    return m_isOpen;
}

bool AravisCameraService::setExposureMs(int exposureMs, QString *errorMessage)
{
    if (exposureMs <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("曝光时间必须大于 0 ms。");
        }
        return false;
    }
    m_exposureMs = exposureMs;
    return applyCameraParameters(errorMessage);
}

bool AravisCameraService::setGain(int gain, QString *errorMessage)
{
    if (gain < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机增益不能小于 0。");
        }
        return false;
    }
    m_gain = gain;
    return applyCameraParameters(errorMessage);
}

bool AravisCameraService::setParameters(int exposureMs, int gain, QString *errorMessage)
{
    if (exposureMs <= 0 || gain < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("相机曝光必须大于 0 ms，增益不能小于 0。");
        }
        return false;
    }
    m_exposureMs = exposureMs;
    m_gain = gain;
    return applyCameraParameters(errorMessage);
}

bool AravisCameraService::recoverStream(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    if (!m_isOpen || !m_camera) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GigE Vision 控制连接已断开，无法单独恢复图像流。");
        }
        return false;
    }
    stopStream();
    return startStream(errorMessage);
#else
    Q_UNUSED(errorMessage);
    return false;
#endif
}

QImage AravisCameraService::grabFrame(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    if (!m_isOpen || !m_camera || !m_stream) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("GigE Vision 相机尚未连接。");
        }
        return {};
    }

    auto *stream = static_cast<ArvStream *>(m_stream);
    const gint64 timeoutUs = std::max<gint64>(
        1500000,
        static_cast<gint64>(std::max(1000, m_exposureMs)) * 1000 + 1000000);
    const gint64 deadlineUs = g_get_monotonic_time() + timeoutUs;
    int rejectedBuffers = 0;
    ArvBufferStatus lastRejectedStatus = ARV_BUFFER_STATUS_SUCCESS;

    while (g_get_monotonic_time() < deadlineUs) {
        ArvBuffer *selectedBuffer = arv_stream_try_pop_buffer(stream);
        if (!selectedBuffer) {
            const gint64 remainingUs = deadlineUs - g_get_monotonic_time();
            selectedBuffer = arv_stream_timeout_pop_buffer(
                stream, static_cast<guint64>(std::max<gint64>(1, remainingUs)));
        }
        if (!selectedBuffer) {
            break;
        }

        auto keepIfNewerAndComplete = [&](ArvBuffer *candidate) {
            const ArvBufferStatus status = arv_buffer_get_status(candidate);
            if (status != ARV_BUFFER_STATUS_SUCCESS) {
                lastRejectedStatus = status;
                ++rejectedBuffers;
                arv_stream_push_buffer(stream, candidate);
                return;
            }
            if (selectedBuffer && selectedBuffer != candidate) {
                arv_stream_push_buffer(stream, selectedBuffer);
            }
            selectedBuffer = candidate;
        };

        ArvBuffer *firstBuffer = selectedBuffer;
        selectedBuffer = nullptr;
        keepIfNewerAndComplete(firstBuffer);
        while (ArvBuffer *queuedBuffer = arv_stream_try_pop_buffer(stream)) {
            keepIfNewerAndComplete(queuedBuffer);
        }
        if (!selectedBuffer) {
            continue;
        }

        QImage frame;
        size_t dataSize = 0;
        const void *data = arv_buffer_get_image_data(selectedBuffer, &dataSize);
        const int width = arv_buffer_get_image_width(selectedBuffer);
        const int height = arv_buffer_get_image_height(selectedBuffer);
        int xPadding = 0;
        int yPadding = 0;
        arv_buffer_get_image_padding(selectedBuffer, &xPadding, &yPadding);
        Q_UNUSED(yPadding);

        const quint64 pixelFormat = static_cast<quint64>(arv_buffer_get_image_pixel_format(selectedBuffer));
        const int bytesPerPixel = pixelFormat == CameraFrameConverter::Rgb8Packed ||
                                          pixelFormat == CameraFrameConverter::Bgr8Packed
                                      ? 3
                                      : CameraFrameConverter::isUnpacked16(pixelFormat) ? 2 : 1;
        frame = CameraFrameConverter::fromPfnc(data,
                                               static_cast<qsizetype>(dataSize),
                                               width,
                                               height,
                                               width * bytesPerPixel + xPadding,
                                               pixelFormat);
        if (frame.isNull() && errorMessage) {
            *errorMessage = QStringLiteral("相机返回了暂不支持的 PFNC 像素格式：0x%1")
                                .arg(pixelFormat, 8, 16, QLatin1Char('0'));
        }

        arv_stream_push_buffer(stream, selectedBuffer);
        return frame;
    }

    if (errorMessage) {
        guint64 completed = 0;
        guint64 failures = 0;
        guint64 underruns = 0;
        arv_stream_get_statistics(stream, &completed, &failures, &underruns);
        if (rejectedBuffers > 0) {
            *errorMessage = QStringLiteral(
                                "等待完整相机帧超时；本次丢弃 %1 个不完整缓冲区（状态 %2），"
                                "累计完整帧 %3、失败帧 %4、缓冲区不足 %5。")
                                .arg(rejectedBuffers)
                                .arg(static_cast<int>(lastRejectedStatus))
                                .arg(completed)
                                .arg(failures)
                                .arg(underruns);
        } else {
            *errorMessage = QStringLiteral(
                                "等待相机图像超时；累计完整帧 %1、失败帧 %2、缓冲区不足 %3。")
                                .arg(completed)
                                .arg(failures)
                                .arg(underruns);
        }
    }
    return {};
#else
    if (errorMessage) {
        *errorMessage = QStringLiteral("工业相机支持组件不可用，请联系系统管理员。");
    }
    return {};
#endif
}

bool AravisCameraService::applyCameraParameters(QString *errorMessage)
{
#ifdef TEA_MONITOR_HAS_ARAVIS
    if (!m_camera || !m_isOpen) {
        return true;
    }

    auto *camera = static_cast<ArvCamera *>(m_camera);
    GError *error = nullptr;
    arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error);
    if (error) {
        if (errorMessage) {
            *errorMessage = consumeError(QStringLiteral("设置连续采集模式失败"), error);
        } else {
            g_error_free(error);
        }
        return false;
    }

    auto *device = arv_camera_get_device(camera);
    const bool usesImperxRawControls = device &&
                                      arv_device_get_feature(device, "ExposureTimeRaw") != nullptr;
    if (usesImperxRawControls) {
        auto reportFeatureError = [errorMessage](const QString &operation, GError *featureError) {
            if (errorMessage) {
                *errorMessage = consumeError(operation, featureError);
            } else if (featureError) {
                g_error_free(featureError);
            }
            return false;
        };
        auto setStringFeature = [device, &reportFeatureError](const char *name,
                                                              const char *value,
                                                              const QString &operation) {
            GError *featureError = nullptr;
            arv_device_set_string_feature_value(device, name, value, &featureError);
            return featureError ? reportFeatureError(operation, featureError) : true;
        };
        auto setBooleanFeature = [device, &reportFeatureError](const char *name,
                                                               gboolean value,
                                                               const QString &operation) {
            GError *featureError = nullptr;
            arv_device_set_boolean_feature_value(device, name, value, &featureError);
            return featureError ? reportFeatureError(operation, featureError) : true;
        };
        auto setIntegerFeature = [device, &reportFeatureError](const char *name,
                                                               gint64 value,
                                                               const QString &operation) {
            GError *featureError = nullptr;
            arv_device_set_integer_feature_value(device, name, value, &featureError);
            return featureError ? reportFeatureError(operation, featureError) : true;
        };

        if (!setStringFeature("TriggerMode", "Off", QStringLiteral("关闭相机触发模式失败")) ||
            !setStringFeature("ExposureMode", "Timed", QStringLiteral("设置相机曝光模式失败")) ||
            !setBooleanFeature("AecEnable", FALSE, QStringLiteral("关闭相机自动曝光失败")) ||
            !setBooleanFeature("ConstantFrameRate", FALSE, QStringLiteral("关闭相机恒定帧率失败"))) {
            return false;
        }

        gint64 exposureRaw = static_cast<gint64>(m_exposureMs) * 1000;
        if (arv_device_get_feature(device, "MaxExposure")) {
            GError *maxExposureError = nullptr;
            const gint64 maxExposure = arv_device_get_integer_feature_value(
                device, "MaxExposure", &maxExposureError);
            if (maxExposureError) {
                g_error_free(maxExposureError);
            } else if (maxExposure > 0) {
                exposureRaw = std::min(exposureRaw, maxExposure);
            }
        }

        if (!setIntegerFeature("ExposureTimeRaw", exposureRaw,
                               QStringLiteral("设置相机曝光时间失败")) ||
            !setBooleanFeature("AgcEnable", FALSE, QStringLiteral("关闭相机自动增益失败")) ||
            !setStringFeature("GainAutoBalance", "Off", QStringLiteral("关闭相机增益自动平衡失败")) ||
            !setStringFeature("GainSelector", "AnalogTap1", QStringLiteral("选择相机增益通道 1 失败")) ||
            !setIntegerFeature("GainRaw", m_gain, QStringLiteral("设置相机增益通道 1 失败")) ||
            !setStringFeature("GainSelector", "AnalogTap2", QStringLiteral("选择相机增益通道 2 失败")) ||
            !setIntegerFeature("GainRaw", m_gain, QStringLiteral("设置相机增益通道 2 失败"))) {
            return false;
        }
    } else {
        arv_camera_set_exposure_time_auto(camera, ARV_AUTO_OFF, &error);
        if (!error) {
            arv_camera_set_exposure_time(camera, static_cast<double>(m_exposureMs) * 1000.0, &error);
        }
        if (error) {
            if (errorMessage) {
                *errorMessage = consumeError(QStringLiteral("设置相机曝光时间失败"), error);
            } else {
                g_error_free(error);
            }
            return false;
        }

        arv_camera_set_gain_auto(camera, ARV_AUTO_OFF, &error);
        if (!error) {
            arv_camera_set_gain(camera, static_cast<double>(m_gain), &error);
        }
        if (error) {
            if (errorMessage) {
                *errorMessage = consumeError(QStringLiteral("设置相机增益失败"), error);
            } else {
                g_error_free(error);
            }
            return false;
        }
    }
    return true;
#else
    Q_UNUSED(errorMessage);
    return true;
#endif
}
