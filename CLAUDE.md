# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

PunchPressVision 是一款基于 C++17 的 Windows 桌面视觉应用，使用 Qt6、OpenCV 和 MVTec Halcon 构建。它采用自定义的 RWUL 框架（oso/core/rqwcm/rqwu）来实现配置管理、相机通信和 UI 组件。项目通过 vcpkg 管理 C++ 依赖。

## 构建系统

**CMake + vcpkg + Ninja。** 需要三个外部 SDK 路径，通过 CMake 缓存变量引用：
- `QT_DIR` — Qt 6.7.3+ 安装路径（msvc2022_64）
- `HALCON_DIR` — MVTec Halcon 24.11+ 安装路径
- `RWUL_DIR` — 自定义 RWUL 框架安装路径

这些路径在 `cmake/CMakeCacheIni.cmake` 中以占位默认值（`D:`）初始化，必须在 `CMakeUserPresets.json` 中覆盖（该文件已被 gitignore）。当前开发者的实际路径请参见工作区中的 `CMakeUserPresets.json`。

### 常用命令

配置（Debug）：
```bash
cmake --preset user-ninja-debug
```

构建：
```bash
cmake --build --preset windows-ninja-debug-build
```

构建指定目标：
```bash
cmake --build --preset windows-ninja-debug-build --target <TargetName>
```

测试在 `BUILD_TESTING=ON` 时构建（用户预设中已设置）。没有统一的测试运行预设，每个模块都会生成独立的测试可执行文件。

### vcpkg

依赖项声明在 `vcpkg.json` 中。项目使用清单模式，`vcpkg-configuration.json` 指向 Microsoft 官方注册表。添加或修改依赖后，重新配置 preset 即可触发 vcpkg 安装。

## 架构

代码库采用分层架构，共五层，对应 `PunchPressVision/` 下的顶级 CMake 子目录：

```
UI/               → Qt 控件、对话框、.ui 文件、.qrc 资源、main.cpp
Business/         → 业务逻辑层，封装 infTool 与 infrastructure 操作
infTool/          → 基于基础设施的可复用视觉算法工具层
infrastructure/   → 硬件抽象、配置管理、数据持久化
global/           → 共享类型和接口（IInfrastructure、IBusiness）
```

### 各层职责

- **global**：定义 `global::IInfrastructure`（`build`/`destroy`）和 `global::IBusiness`（`build`/`destroy`/`start`/`stop`）。最小化的共享类型，如 `global::CameraIndex`。
- **infrastructure**：各模块（如 `CameraModule`、`ConfigModule`、`CalibConfigModule`、`NinePointModule`、`TwoCameraSpliceModule`、`ShapeModelManagerModule`）均继承 `global::IInfrastructure`。聚合类 `inf::infrastructure` 持有每个模块的 `unique_ptr`，并统一编排 `build()`/`destroy()`。
- **infTool**：基于 infrastructure 的可复用视觉算法工具层。当前包含 `infTool::CalibBun`（Halcon 畸变标定）、`infTool::NinePointBun`（九点标定）、`infTool::TwoCameraSpliceBun`（双相机拼接标定）。聚合类 `infTool::infTool` 持有这三个工具的 `unique_ptr`，并统一编排生命周期。`Business` 层通过该聚合访问这些工具。
- **Business**：各 Bundle（如 `CameraBun`、`ShapeModeManagerBun`、`LightControlBun`）均继承 `global::IBusiness`。聚合类 `bun::Business` 持有 `infTool::infTool` 的 `unique_ptr` 以及其余 Bundle，并接收 `inf::infrastructure` 的引用。Bundle 负责将基础设施与 infTool 能力桥接到应用业务流。
- **UI**：基于 Qt 的 UI 层。`PunchPress` 是主窗口类（目前包含大量未激活功能的 `#if 0` 块）。`UIModule` 构建为静态库，最终链接到 `PunchPressVision` 可执行文件。

### 模块规范

每个基础设施模块和业务模块遵循统一的 CMake 和目录结构：

```
ModuleName/
  CMakeLists.txt
  include/ModulePath/ModuleName.hpp
  src/ModuleName.cpp
  test/
    CMakeLists.txt
    include/ModuleName.t.hpp
    src/ModuleName.t.cpp
```

- 目标为静态库，带命名空间别名：`inf::CameraModule`、`infTool::CalibBun`、`bun::CameraBun`、`global::global`、`UI::UIModule`。
- Debug 库通过后缀 `d` 区分（`DEBUG_POSTFIX` 设置）。
- 头文件搜索路径使用 `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>`。
- 每个模块的 `test/` 子目录生成独立测试可执行文件，命名格式为 `Test_inf_<Module>`、`Test_infTool_<Module>` 或 `Test_bun_<Bundle>`。

### 配置系统（OSO）

项目使用 RWUL 的 `oso`（ObjectStore）序列化框架进行持久化配置：
- 配置结构体（如 `Config::BaseCfg`、`Config::CalibConfig`）实现构造函数和 `operator rw::oso::ObjectStoreAssembly()` 用于转换。
- `infrastructure/ConfigModule/osoFile/` 中的 `.oso` 源文件通过 CMake 函数 `oso_wrap_oso()` 生成到头文件目录 `include/infrastructure/ConfigModule/Config/` 下。
- `ConfigModule` 持有 `rw::oso::StorageContext`，对外暴露类型化的配置对象（`baseCfg`、`cameraCfg`、`visionCfg`）。

### 相机与视觉流程

- `CameraModule` 管理以 `global::CameraIndex`（Camera1、Camera2）为键的 `rw::rqwc::MVSCameraPassive` 实例。
- 相机回调发射 Qt 信号（`callBackFunc`、`cameraConnectionStateChanged`），使用 `Qt::DirectConnection` 连接。
- `bun::CameraBun` 通过 `CameraImgConvert` 将 OpenCV 的 `cv::Mat` 转换为 Halcon 的 `HImage`，并转发已校准的图像。
- `infTool::CalibBun` 封装 Halcon 标定 API（`CreateCalibData`、`CalibrateCameras`、`FindCalibObject`），用于畸变校正和标定板标记检测。
- `infTool::TwoCameraSpliceBun` 处理双相机图像拼接标定。
- `infTool::NinePointBun` 实现九点标定，用于像素坐标到世界坐标的转换。

## 主要依赖

| 包 | 用途 |
|---------|---------|
| Qt6 (Core/Widgets/Gui/Concurrent/Network) | UI 框架 |
| OpenCV 4.12 | 图像处理 |
| Halcon 24.11 | 工业视觉算法 |
| RWUL (oso/core/rqwcm/rqwu) | 配置序列化、相机通信、UI 控件 |
| jsoncpp | JSON 处理 |
| sqlite3 | 数据库 |
| gtest | 测试框架 |
| spdlog | 日志 |
| libmodbus | Modbus 通信 |
| libzip | Zip 压缩解压 |

## 开发注意事项

- `CMakeUserPresets.json` 已被 gitignore，每位开发者需在其中维护本地 SDK 路径。
- `App/CMakeLists.txt` 目前内容极少（观察到仅有一行 `add_subdirectory(UIModule)`），可执行目标实际定义在 `UI/CMakeLists.txt` 中。
- `windeployqt` 通过 `cmake/func.cmake` 在构建后自动运行，用于打包 Qt DLL。
- Halcon DLL 通过 `Halcon` 导入目标在构建后自动复制到输出目录。
- 主窗口（`PunchPress`）包含大量 `#if 0` 块；取消注释后会引入额外的 RWUL UI 组件和一个尚未激活的 `ProcessModule`。
- 每个模块都配备了测试文件，但目前大多是空桩，仅包含 `int main() { return 0; }`。
