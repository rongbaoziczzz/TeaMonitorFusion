# OceanDirect SDK 与驱动

## 当前状态

- SDK 已由项目所有者复制到 `sdk/OceanDirect SDK`。
- 已发现开发头文件、链接库、运行库、`winusb_driver` 和 `winusb_driver_offline`。
- TeaMonitorFusion 通过 DLL 的官方 C 接口运行时加载，可兼容当前 MinGW 构建。

## 许可说明

本机附带的 `OceanDirect_EULA.txt` 将软件许可限定为客户内部使用，禁止未经授权复制、转让或向第三方开放，并限制同时安装使用的设备数量。将完整 SDK 或驱动直接纳入项目包可能违反该许可。

## 支持的放置方式

取得 Ocean Optics 的书面再分发授权后，将官方 SDK 内容按以下结构放置：

```text
OceanDirect/sdk/
|-- include/api/OceanDirectAPI.h
|-- lib/OceanDirect.lib
|-- lib/OceanDirect.dll
|-- winusb_driver/
`-- winusb_driver_offline/
```

CMake 同时检测 `OceanDirect/sdk` 和当前的 `OceanDirect/sdk/OceanDirect SDK` 嵌套结构。构建后会自动复制 `OceanDirect.dll` 与 `NetOceanDirect.dll` 到程序目录；设备驱动仍应通过官方方式安装。
