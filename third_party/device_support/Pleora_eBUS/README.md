# Pleora eBUS SDK 与驱动（历史说明）

当前项目已改用 LGPL 开源的 Aravis 接入 IMPERX GigE Vision 相机，本目录不再参与构建，也不需要购买 eBUS SDK。

## 当前状态

- 本机仅发现 `C:/Program Files/Pleora Technologies Inc/eBUS Player`。
- 未发现完整 eBUS SDK、开发头文件、链接库或独立驱动安装包。
- `eBUS Player` 不能替代开发 SDK，因此未复制到项目中。

## 官方获取入口

- 产品页：[Pleora eBUS SDK](https://www.pleora.com/machine-vision-connectivity/ebus-sdk/)
- 官方支持中心：[eBUS SDK 主题](https://supportcenter.pleora.com/s/topic/0TO340000004X6dGAE/ebus-sdk)
- 官方商城：[eBUS SDK](https://www.buypleora.com/collections/ebus-sdk)

官方页面未提供无需登录或授权即可直接下载的 SDK 安装包。请使用已购买产品或客户支持账户获取与相机型号、Windows 架构和 Visual Studio 版本匹配的安装包。

## 获得 SDK 后的放置方式

请从 Pleora 官方安装程序获取与相机和编译器匹配的完整 eBUS SDK。不要把 `eBUS Player` 目录改名后当作 SDK。取得授权后，将 SDK **内容** 放入以下目录，确保 `PvSystem.h` 直接位于 `Includes` 下：

```text
Pleora_eBUS/eBUS_SDK/
|-- Includes/PvSystem.h
`-- Libraries/
    |-- PvBase.lib
    |-- PvDevice.lib
    |-- PvStream.lib
    |-- PvBuffer.lib
    `-- PvGenICam.lib
```

CMake 会优先检测该目录。设备驱动仍应通过与 SDK/相机型号匹配的官方安装程序安装。

## 构建方式

当前 eBUS 适配代码使用 Pleora 的 C++ API，不能用 MinGW 直接链接 MSVC 版本的 eBUS `.lib`。请使用 Qt MSVC 套件，并在 Visual Studio Developer PowerShell 中执行：

```powershell
$env:QT_ROOT = "E:/Qt/6.10.1/msvc2022_64"
cmake --preset windows-msvc-release `
  -DTEA_MONITOR_EBUS_ROOT="E:/MyProject/Qt Program/TeaMonitorFusion/third_party/device_support/Pleora_eBUS/eBUS_SDK"
cmake --build --preset windows-msvc-release
```

如果 SDK 的运行 DLL 不在 `Bin`、`bin` 或 `Libraries/Win64_x64*` 中，再额外指定：

```powershell
cmake --preset windows-msvc-release `
  -DTEA_MONITOR_EBUS_RUNTIME_DIR="D:/Pleora/eBUS SDK/Bin/Win64_x64"
```

配置输出中的 `eBUS: ON` 才表示已经找到完整开发包。目标电脑仍需通过官方驱动安装程序安装相机驱动和网络组件。
