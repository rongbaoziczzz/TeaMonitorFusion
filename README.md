# 茶叶监测系统重构版

`TeaMonitorFusion` 是一个基于 Qt 6 的桌面应用，用于将光谱仪检测流程与工业相机检测流程整合到统一入口中，方便演示、联调、培训和后续真实设备接入。

当前项目重点解决的是“统一入口、独立检测界面、设备适配分层”这三件事，不直接改动旧工程，而是在独立仓库中完成重构验证。

## 项目特点

- 统一启动入口，先选设备，再进入对应检测界面
- 将光谱仪流程与工业相机流程拆分为独立工作界面
- 为不同厂商 SDK 预留独立服务层，便于后续替换真实设备实现
- 提供光谱模拟器和工业相机模拟器，便于无硬件环境下调试
- 界面文案以中文为主，适合现场演示和培训使用

## 当前支持的设备方向

### 光谱仪

- 海洋光学光谱仪
  - 波段范围：`1000-1700 nm`
  - SDK：`OceanDirect SDK`
  - 说明：通过 OceanDirect DLL 的官方 C 接口运行时加载，支持现有 `Qt 6.10.1 + MinGW` 构建，不依赖 MSVC `.lib` ABI

- 如海广电光谱仪
  - 波段范围：以设备标定波长为准（当前 XS11639 实测约 `343.7-1070.4 nm`）
  - SDK：`SeaSDK`
  - 说明：已保留原项目中的接入思路，并保留积分时间、平滑、平均次数、实时采集等能力

- 光谱模拟器
  - 用途：无设备时用于演示、联调和培训

### 工业相机

- IMPERX B1621C 工业相机（`GEV-B1621C-TC000`）
  - 接口：`GigE Vision / GenICam`
  - SDK 方向：`Aravis 0.8`
  - 说明：默认按序列号 `56S014` 绑定，支持曝光、增益、连续取帧和 Mono8/10/12、Bayer8/10/12、RGB8 图像转换，不依赖 Pleora eBUS 授权

- 工业相机模拟器
  - 用途：用于流程演示、界面联调和非现场开发

## 当前界面能力

- 启动页展示设备名称、波段范围和对应 SDK
- 光谱检测界面支持：
  - 连接设备
  - 开始检测 / 停止检测
  - 积分时间、平滑宽度、平均次数调整
  - 光照强度、反射率、透射率模式切换
  - 反射率暗/白参考与透射率暗/透射参比分别采集
  - 光谱 CSV 导出，包含原始强度、反射率和透射率字段

- 工业相机检测界面支持：
  - 连接设备
  - 开始检测 / 停止检测
  - 曝光时间与增益调整
  - 实时画面预览
  - 当前图像导出

- 子界面顶部支持“返回首页”，便于重新选择设备

## 构建环境

- Qt：`6.10.1`
- 编译器：`MinGW`
- CMake：`>= 3.16`
- C++ 标准：`C++17`

## 本地构建

项目内已建立统一设备支持目录 `third_party/device_support`。构建时优先使用其中已完整放置的 SDK；如需指定其他位置，可通过 `-DTEA_MONITOR_*_ROOT` 覆盖，环境变量仅作为项目内目录不存在时的回退：

```powershell
$env:QT_ROOT = "E:/Qt/6.10.1/mingw_64"
$env:SEASDK_ROOT = "D:/SDK/SeaSDK"           # 项目内目录不存在时使用
$env:OCEANDIRECT_ROOT = "D:/SDK/OceanDirect" # 项目内目录不存在时使用
$env:ARAVIS_ROOT = "D:/SDK/Aravis"           # 项目内目录不存在时使用
```

没有对应 SDK 时仍可构建模拟器版本。配置阶段会明确输出 SeaSDK、OceanDirect、Aravis 和模拟器的启用状态；也可以通过 `TEA_MONITOR_REQUIRE_*` 选项要求缺少指定 SDK 时直接终止构建。

```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

显式指定外部 SDK 的示例：

```powershell
cmake -S . -B build -DTEA_MONITOR_SEASDK_ROOT="D:/SDK/SeaSDK"
```

构建完成后执行测试和生成可部署目录：

```powershell
ctest --test-dir build --output-on-failure
cmake --install build --prefix release
```

构建完成后，程序位于：

- `build/TeaMonitorFusion.exe`

也可以直接运行仓库中的批处理脚本：

```powershell
.\build_and_run.bat
```

## 项目结构

```text
TeaMonitorFusion/
|-- assets/                 图标与静态资源
|-- third_party/
|   `-- device_support/     按厂商分开的 SDK、驱动接入目录与授权说明
|-- src/
|   |-- models/             设备定义
|   |-- services/           光谱仪 / 相机服务适配层
|   |-- ui/                 应用主题与样式
|   |-- widgets/            自定义界面组件
|   |-- launcherwindow.*    启动页
|   |-- mainwindow.*        检测主界面
|   `-- main.cpp            程序入口
|-- CMakeLists.txt
`-- build_and_run.bat
```

## 驱动与 SDK 整理状态

- `SeaSDK`：已整理到 `third_party/device_support/SeaSDK`，项目不再依赖旧工程目录。
- `OceanDirect`：SDK 已放入 `third_party/device_support/OceanDirect/sdk`。CMake 同时兼容直接内容和 `sdk/OceanDirect SDK` 嵌套结构，并自动把两个运行库部署到程序目录。
- `Aravis`：官方 LGPL 源码快照已放入 `third_party/device_support/Aravis/aravis-0.8.36`。按 `Aravis/README.md` 准备 MinGW 开发包后，CMake 自动启用真实 GigE Vision 相机。
- Pleora eBUS 不再参与当前程序的构建和运行；原说明目录仅保留作历史对照。
- 厂商二进制和头文件默认被 `.gitignore` 排除，防止随源码仓库误提交。交付项目文件夹前仍需确认各厂商授权是否允许随产品交付。

## 当前限制与说明

- `OceanDirect SDK` 使用 C 接口动态加载以兼容 MinGW；目标电脑仍需正确安装对应设备驱动和 Microsoft Visual C++ 运行库
- IMPERX GigE Vision 相机要实现真实接入，目标机器需要 Aravis/GLib 运行 DLL，并正确配置相机网卡 IPv4 与防火墙
- 仓库中的模拟器更适合开发和演示，不代表已经完成所有真实硬件联调

## 生产化能力

- 设备连接和采集运行在独立工作线程，界面不会因 SDK 等待而阻塞
- 支持独立光谱、独立相机以及同一任务下的光谱与图像融合检测
- 连续三次采集失败后进入重连状态，并保留明确的故障日志
- 暗、白参考绑定设备、波长轴和采集参数，匹配后生成反射率数据
- CSV 和图像导出包含样品、批次、操作员、设备与采集参数；文件采用原子写入
- 参数、操作员和导出目录通过系统设置持久化
- 运行日志保存在系统应用数据目录的 `TeaMonitorFusion/logs` 下，默认保留 14 天
- 每次成功导出都会写入本地 `detection_records.sqlite` 审计数据库
- 当检测到多台同类真实设备但没有序列号绑定时，系统拒绝自动选择第一台设备

## 后续建议

如果下一步准备把项目推进到真实生产环境，建议优先确认以下两件事：

1. Aravis/GLib 开发包是否与当前 Qt MinGW 使用同一 64 位 ABI
2. 目标电脑是否已部署 Aravis/GLib 运行 DLL，并完成相机网卡 IPv4 配置
