# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

PunchPressVision 是一款基于 C++17 的 Windows 桌面视觉应用，使用 Qt6、OpenCV 和 MVTec Halcon 构建。它采用自定义的 RWUL 框架（oso/core/rqwcm/rqwu）来实现配置序列化、相机通信和 UI 控件。项目通过 vcpkg 清单模式管理 C++ 依赖。

## 构建系统

**CMake + vcpkg + Ninja。**

### 外部 SDK

构建需要三个外部 SDK，通过 CMake 缓存变量引用：

- `QT_DIR` — Qt 6.7.3+ 安装路径（`msvc2022_64`）
- `HALCON_DIR` — MVTec Halcon 24.11+ 安装路径
- `RWUL_DIR` — 自定义 RWUL 框架安装路径

`cmake/CMakeCacheIni.cmake` 中以占位默认值（`D:`）初始化这些变量，因此**必须在 `CMakeUserPresets.json` 中覆盖**。该文件已被 gitignore，每位开发者需自行维护本地路径。

最小示例：

```json
{
  "version": 4,
  "configurePresets": [
    {
      "name": "user-ninja-debug",
      "inherits": "windows-ninja-debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "BUILD_TESTING": true,
        "QT_DIR": "C:/path/to/Qt/6.7.3/msvc2022_64",
        "RWUL_DIR": "C:/path/to/RWUL",
        "HALCON_DIR": "C:/path/to/HALCON-24.11-Progress-Steady"
      }
    }
  ]
}
```

### 环境变量

配置预设使用 `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` 作为工具链，因此需要设置：

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
```

### 常用命令

配置（Debug）：

```bash
cmake --preset user-ninja-debug
```

构建全部（从已配置的 build 目录）：

```bash
cmake --build out/build/user-ninja-debug
```

构建指定目标：

```bash
cmake --build out/build/user-ninja-debug --target PunchPressVision
```

Release 构建使用 `user-ninja-release` 和 `out/build/user-ninja-release`。

### 构建产物

运行时输出目录固定为：

```
output/<Debug|Release>/PunchPressVision/
```

- 主程序：`PunchPressVision.exe`
- 测试可执行文件：`Test_inf_*`、`Test_infTool_*`、`Test_bun_*`、`Test_app_*`
- 独立工具：`ToolCalibDistortion.exe`、`ToolNinePoint.exe`、`ToolTwoCameraSplice.exe`

构建后自动执行：

- `windeployqt` 为可执行目标打包 Qt DLL。
- Halcon DLL 复制到 `PunchPressVision.exe` 输出目录。
- `build.version` 复制到所有需要版本一致性检查的可执行文件输出目录。

Release 模式会定义 `PPV_RELEASE_FULLSCREEN`，主窗口以全屏无边框方式启动。

### 运行

```powershell
output/Debug/PunchPressVision/PunchPressVision.exe
```

测试没有统一的 CTest 预设，当前为独立可执行文件。构建并运行单个测试：

```bash
cmake --build out/build/user-ninja-debug --target Test_inf_CameraModule
output/Debug/PunchPressVision/Test_inf_CameraModule.exe
```

## 版本与构建 ID

CMake 在根目录生成版本信息：

- `PPV_VERSION_STRING`：由 `MAJOR.MINOR.PATCH.BUILD` 组成（当前 `1.0.0.0`）。
- `PPV_BUILD_ID`：`<时间戳>_<git 短哈希>`。

生成产物：

- `PunchPressVision/global/include/global/BuildVersion.hpp.in` 被配置到 `${CMAKE_BINARY_DIR}/PunchPressVision/global/include/global/BuildVersion.hpp`。
- `${CMAKE_BINARY_DIR}/build.version` 写入构建 ID，并在构建后复制到可执行文件目录。

`global::VersionChecker` 在 `main()` 启动时比较嵌入到二进制中的构建 ID 与 `build.version`。不一致时弹窗提示用户更新或退出，防止同一工作目录混用不同版本的 EXE。

## 架构

代码库采用分层架构，对应 `PunchPressVision/` 下的顶级 CMake 子目录：

```
global/           → 共享类型与接口（IInfrastructure、IBusiness、IInfTool、CameraIndex、RunMode 等）
infrastructure/   → 硬件抽象、配置管理、数据持久化
infTool/          → 基于基础设施的可复用视觉算法工具层
Business/         → 业务逻辑层（Bundle），桥接 infTool 与 App
App/              → 应用状态机与启动/就绪检查（PunchPressApp）
UI/               → Qt 控件、对话框、.ui 文件、.qrc 资源、main.cpp
```

### 全局接口

`global::IInfrastructure` 提供 `build()` / `destroy()`。
`global::IBusiness` 提供 `build()` / `destroy()` / `start()` / `stop()`。
`global::IInfTool` 继承 `QObject` 与 `IBusiness`，用于视觉算法工具类。

### infrastructure 层

聚合类 `inf::infrastructure` 持有各模块的 `unique_ptr`：

- `ConfigModule`：基于 oso 的配置持久化，暴露 `baseCfg`、`cameraCfg`、`visionCfg` 等类型化配置对象。
- `CameraModule`：管理以 `global::CameraIndex`（Camera1 / Camera2）为键的 `rw::rqwc::MVSCameraPassive` 实例。
- `CalibConfigModule`、`NinePointModule`、`TwoCameraSpliceModule`：标定结果/数据管理。
- `ShapeModelManagerModule`：模板模型管理。
- `ControlModule`、`LightIOModule`：PLC/IO 控制与光源控制。

所有模块在 `inf::infrastructure::build()` 中统一构建，在 `destroy()` 中逆序销毁。

### infTool 层

聚合类 `infTool::infTool` 持有三个视觉算法工具：

- `CalibInfTool`：Halcon 相机畸变标定与矫正。
- `NinePointInfTool`：九点标定，像素坐标到世界坐标转换。
- `TwoCameraSpliceInfTool`：双相机图像拼接/平铺标定。

这些工具继承 `global::IInfTool`，内部使用 `inf::infrastructure` 中的配置与相机数据。

### Business 层

聚合类 `bun::Business` 持有 `infTool::infTool` 引用以及多个 Bundle：

- `CameraBun`：桥接已处理图像帧流到 App 层，转发 `TwoCameraSpliceInfTool` 输出的 `HalconCpp::HImage`。
- `ShapeModeManagerBun`：模板模型业务封装。
- `LightControlBun`：光源控制业务封装。
- `HealthMonitorBun`：健康监控。

`bun::Business` 还提供了 `CalibBun` / `NinePointBun` / `TwoCameraSpliceBun` 等别名，直接指向 infTool 工具实例。

### App 层

`app::PunchPressApp` 是应用层聚合根，职责包括：

- 启动检查（单实例、MVS 进程冲突、配置完整性）。
- 运行模式状态机（Idle / Debug / Production / CreateModel / DrawMatchRegion）。
- 就绪性查询（标定就绪、生产就绪）。
- 生产帧处理编排（匹配 + 写 PLC）。

### UI 层

`PunchPressVision/UI/CMakeLists.txt` 定义最终可执行目标 `PunchPressVision`，源文件位于 `UI/src/main.cpp`。当前 UI 主要实现为 `PunchPressVision/UI/PunchPressUI/` 中的 `ui::PunchPress`，通过 `app::PunchPressApp` 与下层交互。旧 `UIModule` 已被注释掉，不再链接进可执行文件。

### 启动时序

`main.cpp` 中的生命周期：

```
Phase 1: 构造
  inf::infrastructure infrastructure;
  bun::Business business(infrastructure);
  app::PunchPressApp app(business);
  ui::PunchPress w(app);

Phase 2: build
  infrastructure.build() → business.build() → app.build() → w.build()

Phase 3: start
  business.start() → app.start()

Phase 4: 显示并运行事件循环
  w.show() / showFullScreen() → a.exec()

Phase 5: stop
  app.stop() → business.stop()

Phase 6: destroy
  w.destroy() → app.destroy() → business.destroy() → infrastructure.destroy()
```

`main()` 中还需注册 `HalconCpp::HImage` 与 `global::CameraIndex` 到 Qt 元类型系统，因为帧管线通过 `Qt::QueuedConnection` 跨线程传递 `HImage`。

### 图像与帧流

1. `CameraModule` 接收相机回调（`rw::hoec::MatInfo`，可能在工作线程）。
2. `CalibInfTool` 对单帧做畸变矫正。
3. `TwoCameraSpliceInfTool` 将两路相机图像拼接/平铺为单张 `HalconCpp::HImage`。
4. `CameraBun::onSplicedFrame` 接收处理后的图像并通过 `callBackFunWithCalib(HalconCpp::HImage)` 发射信号。
5. `PunchPressApp` 接收信号后，在 `processProductionFrame` 中执行模板匹配并将结果写入 PLC。
6. UI 通过 `frameReady`、`positionResultReady` 等信号刷新显示。

## 模块规范

每个基础设施/业务模块遵循统一目录结构：

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

- 目标为静态库，带命名空间别名，如 `inf::CameraModule`、`infTool::CalibInfTool`、`bun::CameraBun`、`global::global`、`UI::PunchPressUI`、`app::PunchPressApp`。
- Debug 库通过后缀 `d` 区分（`DEBUG_POSTFIX` 设置）。
- 头文件搜索路径使用 `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>`。
- 测试目录在 `BUILD_TESTING=ON` 时生成独立测试可执行文件，命名格式为 `Test_inf_<Module>`、`Test_infTool_<Module>`、`Test_bun_<Bundle>`、`Test_app_<App>`。

## 独立工具程序

部分 infTool 模块提供独立的 GUI 工具程序，位于 `tool/` 子目录下，供开发者在集成到主应用前独立验证算法：

| 工具 | 路径 | 说明 |
|------|------|------|
| `ToolCalibDistortion` | `infTool/CalibInfTool/tool/ToolCalibDistortion/` | 相机畸变标定与矫正工具（基于 OpenCV）。支持实时相机预览、棋盘格/圆点标定板标定、角点检测预览、YAML 参数存取及实时畸变矫正对比。 |
| `ToolNinePoint` | `infTool/NinePointInfTool/tool/ToolNinePoint/` | 九点标定工具。 |
| `ToolTwoCameraSplice` | `infTool/TwoCameraSpliceInfTool/tool/ToolTwoCameraSplice/` | 双相机拼接标定工具。 |

工具目录遵循精简的 CMake 可执行文件结构，目标名与目录名一致。`.ui` 文件通过 `qt_wrap_ui` 手动编译为 `ui_*.h`（根 CMake 未开启 `AUTOUIC`）。

## 配置系统（OSO）

项目使用 RWUL 的 `oso`（ObjectStore）序列化框架进行持久化配置：

- 配置结构体（如 `Config::BaseCfg`、`Config::cameraCfg`）实现到 `rw::oso::ObjectStoreAssembly` 的转换构造函数或 `operator`。
- `infrastructure/ConfigModule/osoFile/` 中的 `.oso` 源文件通过 CMake 函数 `oso_wrap_oso()` 生成到头文件目录 `include/infrastructure/ConfigModule/Config/` 下。
- `ConfigModule` 持有 `rw::oso::StorageContext`，对外暴露类型化的配置对象，并通过 `save()` 持久化。

## 主要依赖

| 包 | 用途 |
|---------|---------|
| Qt6 (Core/Widgets/Gui/Concurrent/Network) | UI 框架 |
| OpenCV 4.12 | 图像处理 |
| Halcon 24.11 | 工业视觉算法 |
| RWUL (oso/core/rqwcm/rqwu/hoecm/hoepModbus/lgm) | 配置序列化、相机通信、UI 控件、Modbus |
| jsoncpp | JSON 处理 |
| sqlite3 | 数据库 |
| gtest | 测试框架（当前测试多为空桩） |
| spdlog | 日志 |
| libmodbus | Modbus 通信 |
| libzip | Zip 压缩解压 |
| pugixml / openssl | XML 与 TLS 相关 |

## 开发注意事项

- `CMakeUserPresets.json` 已被 gitignore，需在本地维护 Qt / Halcon / RWUL 路径。
- 根目录没有 `README.md`。
- 没有 Cursor 规则（`.cursor/rules/` 或 `.cursorrules`）或 Copilot 规则（`.github/copilot-instructions.md`）。
- 每个模块都配备了测试文件，但目前大多是空桩或仅做简单集成验证。
- 主窗口历史代码中包含大量 `#if 0` 块，取消注释前需确认对应 RWUL UI 组件和 `ProcessModule` 是否已就绪。
- 源码与注释包含中文，根 CMake 通过 `/utf-8` 强制 MSVC 以 UTF-8 编译，避免 C4819 或续行符错误。
