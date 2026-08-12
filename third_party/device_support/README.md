# 设备驱动与 SDK 统一目录

本目录集中管理 TeaMonitorFusion 使用的设备厂商 SDK、运行库和驱动接入说明，各厂商内容必须分目录保存。

```text
device_support/
|-- SeaSDK/                 如海广电光谱仪 SDK
|-- OceanDirect/            Ocean Optics 光谱仪 SDK/WinUSB 驱动接入位
`-- Pleora_eBUS/            Pleora eBUS 相机 SDK/驱动接入位
```

注意：设备驱动通常需要通过厂商安装程序写入 Windows 驱动存储和注册表，仅复制文件不能替代正式安装。SDK 和驱动能否随项目交付，以厂商许可证或书面授权为准。
