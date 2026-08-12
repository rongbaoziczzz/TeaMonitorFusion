# SeaSDK

## 当前状态

- 已从本机旧工程整理到本目录。
- 版本来源目录：`SeaSDK 1.2.0/[6] DLL and SO`。
- 当前包含 `include` 和 `bin`，其中 `bin` 按 Windows/Linux 与体系结构分开。
- CMake 会优先把本目录识别为 `TEA_MONITOR_SEASDK_ROOT`。

## 目录要求

```text
SeaSDK/
|-- include/SeaSDKWrapper.h
`-- bin/
    |-- windows 32bit/SeaSDK.dll, SeaSDK.lib
    `-- windows 64bit/SeaSDK.dll, SeaSDK.lib
```

原文件中未发现明确的许可证文件。当前整理仅用于本机项目开发；提交仓库或向第三方交付前，应向厂商确认复制和再分发权限。
