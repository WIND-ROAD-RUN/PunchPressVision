# PunchPressVision 需求规格说明书

> **版本**: 0.1  
> **更新日期**: 2026-06-18  
> **项目类型**: 工业视觉检测系统（冲压件缺陷检测与定位）  
> **目标平台**: Windows 10+ x64  

---

## 目录

1. [项目概述](#1-项目概述)
2. [系统架构](#2-系统架构)
3. [功能需求总览](#3-功能需求总览)
4. [启动与系统检查](#4-启动与系统检查)
5. [运行模式](#5-运行模式)
6. [相机与控制](#6-相机与控制)
7. [光源控制](#7-光源控制)
8. [视觉标定](#8-视觉标定)
9. [形状模板管理](#9-形状模板管理)
10. [生产检测](#10-生产检测)
11. [UI 交互](#11-ui-交互)
12. [配置与持久化](#12-配置与持久化)
13. [独立工具程序](#13-独立工具程序)
14. [非功能需求](#14-非功能需求)
15. [外部接口](#15-外部接口)
16. [数据模型附录](#16-数据模型附录)

---

## 1. 项目概述

### 1.1 项目背景

PunchPressVision 是一款面向冲压件生产线的工业视觉检测系统。系统通过双相机采集工件图像，经 Halcon 视觉算法进行畸变矫正、图像拼接、模板匹配，实现对冲压件的定位与缺陷检测，并通过 Modbus/PLC 将检测结果发送给执行机构。

### 1.2 应用场景

- **生产模式**：在线实时检测，硬件触发采集，模板匹配定位，PLC 输出偏移量
- **调试模式**：离线查看实时画面，调整相机参数与光源
- **模板管理**：创建、编辑、管理形状匹配模板
- **标定工具**：畸变标定、九点标定、双相机拼接标定已下沉至 infTool 层，通过独立工具程序完成

### 1.3 核心能力

| 能力域 | 说明 |
|--------|------|
| 双相机采集 | 支持 2 路 Hikrobot 工业相机的自由运行与硬件触发采集 |
| 畸变矫正 | 基于 Halcon 的相机标定与实时畸变校正 |
| 双相机拼接 | 双相机图像的几何校正与水平拼接 |
| 九点标定 | 像素坐标到世界坐标（机器人坐标系）的变换 |
| 形状模板匹配 | 基于 Halcon Shape Model 的模板创建、管理、多模型匹配 |
| PLC 通信 | 通过 Modbus TCP 与 PLC 交互，输出检测结果 |
| 光源控制 | 上下光源的独立开关控制 |

### 1.4 技术栈

| 类别 | 技术 |
|------|------|
| 语言 | C++17 |
| UI 框架 | Qt 6.7.3 (Widgets) |
| 视觉算法 | MVTec Halcon 24.11 |
| 图像处理 | OpenCV 4.12.0 |
| 工业通信 | Modbus TCP (libmodbus 3.1.11) |
| 配置序列化 | RWUL OSO 框架 |
| 构建系统 | CMake + vcpkg + Ninja |
| 编译器 | MSVC 2022 (64-bit) |

---

## 2. 系统架构

### 2.1 五层分层架构

```
┌─────────────────────────────────────┐
│  UI 层 (Qt Widgets)                 │  ← 只依赖 App 层
├─────────────────────────────────────┤
│  App 层                             │  ← 只依赖 Business 层
│  应用服务（生命周期管理、跨模块协调）  │
├─────────────────────────────────────┤
│  Business 层 (Bundle)               │  ← 只依赖 Infrastructure 层 + infTool 层
│  业务编排：检测流水线、设备控制       │
├─────────────────────────────────────┤
│  infTool 层                          │  ← 只依赖 Infrastructure 层
│  可复用视觉算法（标定、拼接、转换）    │
├─────────────────────────────────────┤
│  Infrastructure 层 (Module)          │  ← 只依赖 global 层
│  硬件/数据：相机、PLC、配置           │
├─────────────────────────────────────┤
│  global 层                          │  ← 不依赖任何其他层
│  共享类型、常量、接口定义             │
└─────────────────────────────────────┘
```

### 2.2 模块清单

#### global 层

| 模块 | 职责 |
|------|------|
| `global::global` | 共享枚举（CameraIndex, RunMode, TriggerSource, LightState）、接口定义（IInfrastructure, IBusiness, IInfTool）、路径常量、就绪性检查结构体、定位结果结构体 |

#### Infrastructure 层（8 个模块）

| 模块 | 别名 | 职责 |
|------|------|------|
| `ConfigModule` | `inf::ConfigModule` | 集中配置管理，持有 baseCfg / cameraCfg / plcAddressCfg / visionCfg |
| `CameraModule` | `inf::CameraModule` | 双相机（Hikrobot MVS）管理，支持自由运行/触发/单帧采集 |
| `CalibConfigModule` | `inf::CalibConfigModule` | 畸变标定参数（相机内参、外参、标定板）持久化 |
| `NinePointModule` | `inf::NinePointModule` | 九点标定参数与 HomMat2D 变换矩阵持久化 |
| `TwoCameraSpliceModule` | `inf::TwoCameraSpliceModule` | 双相机拼接参数（映射图、校正尺寸）持久化 |
| `ShapeModelManagerModule` | `inf::ShapeModelManagerModule` | 形状模板模型的 CRUD 与文件持久化 |
| `ControlModule` | `inf::ControlModule` | Modbus TCP 网关，PLC 寄存器读写 |
| `LightIOModule` | `inf::LightIOModule` | 上下光源 IO 控制（依赖 ControlModule） |

#### infTool 层（3 个模块）

| 模块 | 别名 | 职责 |
|------|------|------|
| `CalibInfTool` | `infTool::CalibInfTool` | Halcon 畸变标定算法（标定板检测、内参标定、实时畸变矫正） |
| `NinePointInfTool` | `infTool::NinePointInfTool` | 九点手眼标定算法（像素→世界 HomMat2D 计算、PLC 协同自动标定） |
| `TwoCameraSpliceInfTool` | `infTool::TwoCameraSpliceInfTool` | 双相机几何拼接算法（世界平面映射、运行时拼接/直通） |

#### Business 层（4 个 Bundle）

| 模块 | 别名 | 职责 |
|------|------|------|
| `CameraBun` | `bun::CameraBun` | 相机图像信号桥接（从 infTool 图像管道向 App/UI 转发） |
| `ShapeModeManagerBun` | `bun::ShapeModeManagerBun` | 形状模板匹配系统（模型 CRUD、批量加载、匹配推理、用户偏移） |
| `LightControlBun` | `bun::LightControlBun` | 光源控制业务（上下光源开关与状态查询） |
| `HealthMonitorBun` | `bun::HealthMonitorBun` | 系统健康监控（PLC 连接、相机连接定时轮询） |

#### App 层

| 模块 | 职责 |
|------|------|
| `app::PunchPressApp` | 应用聚合根：启动检查、模式切换状态机、生产帧处理（匹配→坐标转换→PLC 输出）、信号桥接 |

#### UI 层

| 模块 | 职责 |
|------|------|
| `UI::PunchPressUI` | 主窗口（PunchPress）、模型编辑器对话框、模型管理器对话框、偏移编辑器对话框 |
| `UI::HalconWidgets` | 可复用 Halcon 显示控件（HalconDisplayLabel、HalconInteractiveLabel、ImageViewer 等） |

### 2.3 数据流

```
相机采集 (cv::Mat)
  → CameraModule::callBackFunc (DirectConnection)
    → CalibInfTool::onCameraFrame (cv::Mat→HImage + 畸变矫正)
      → CalibInfTool::callBackFunc (矫正后 HImage)
        → TwoCameraSpliceInfTool::onCalibFrame (拼接/直通)
          → TwoCameraSpliceInfTool::callBackFunc (拼接后 HImage)
            → CameraBun::onSplicedFrame (信号转发)
              → CameraBun::callBackFunWithCalib
                → PunchPressApp::onFrameReady
                  → [生产模式] processProductionFrame (多模型匹配→坐标转换→PLC写入)
                  → [调试/其他] frameReady 信号 → UI 显示
```

### 2.4 生命周期

```
Phase 1: 构造    → 创建对象、连接 Qt 信号（建立信号依赖图）
Phase 2: build   → infrastructure.build() → business.build() → app.build()
                   （加载配置、连接硬件、创建 Halcon 资源）
Phase 3: start   → business.start() → app.start()
                   （连接 PLC、启动帧监控、开启工作线程）
Phase 4: show    → w.show() → a.exec()
Phase 5: stop    → app.stop() → business.stop()
                   （断开 PLC、停止帧流、停止线程）
Phase 6: destroy → app.destroy() → business.destroy() → infrastructure.destroy()
                   （断开信号、释放资源、保存配置）
```

---

## 3. 功能需求总览

### 3.1 需求编号体系

| 编号段 | 功能域 |
|--------|--------|
| FR-001 ~ FR-004 | 启动与系统检查 |
| FR-005 ~ FR-007 | 调试模式 |
| FR-008 ~ FR-010 | 工作（生产）模式 |
| FR-011 ~ FR-014 | 畸变矫正（infTool 层，独立工具） |
| FR-015 ~ FR-018 | 九点标定（infTool 层，独立工具） |
| FR-019 ~ FR-020 | 双相机拼接（infTool 层，独立工具） |
| FR-021 ~ FR-023 | 光源控制 |
| FR-024 ~ FR-035 | 形状模板管理 |
| FR-036 ~ FR-040 | 相机参数配置 |
| FR-041 ~ FR-045 | 运行模式切换 |
| FR-046 ~ FR-050 | 系统健康监控 |

### 3.2 需求矩阵

| 需求编号 | 需求名称 | 优先级 | 状态 |
|----------|---------|--------|------|
| FR-001 | MVS 进程冲突检测 | P0 | 已实现 |
| FR-002 | 单实例锁 | P0 | 已实现 |
| FR-003 | 配置文件完整性校验 | P1 | 桩代码 |
| FR-004 | 标定参数就绪性检查 | P1 | 已实现 |
| FR-005 | 调试模式-自由运行采集 | P0 | 已实现 |
| FR-006 | 调试模式-实时画面显示 | P0 | 已实现 |
| FR-007 | 调试模式-参数即时调节 | P0 | 已实现 |
| FR-008 | 工作模式-硬件触发采集 | P0 | 已实现 |
| FR-009 | 工作模式-前置条件检查 | P0 | 已实现 |
| FR-010 | 工作模式-定位结果输出 | P0 | 已实现 |
| FR-011 | 畸变标定-标定板检测（infTool） | P0 | 已实现 |
| FR-012 | 畸变标定-内参计算（infTool） | P0 | 已实现 |
| FR-013 | 畸变标定-实时矫正预览（infTool） | P0 | 已实现 |
| FR-014 | 畸变标定-结果持久化（infTool） | P0 | 已实现 |
| FR-015 | 九点标定-前置条件检查（infTool） | P0 | 已实现 |
| FR-016 | 九点标定-像素坐标采集（infTool） | P1 | 部分实现 |
| FR-017 | 九点标定-PLC 协同自动标定（infTool） | P1 | 已实现 |
| FR-018 | 九点标定-坐标变换（infTool） | P0 | 已实现 |
| FR-019 | 双相机拼接-标定计算（infTool） | P0 | 已实现 |
| FR-020 | 双相机拼接-运行时拼接（infTool） | P0 | 已实现 |
| FR-021 | 上光源控制 | P0 | 已实现 |
| FR-022 | 下光源控制 | P0 | 已实现 |
| FR-023 | 光源状态查询 | P0 | 已实现 |
| FR-024 | 模板创建 | P0 | 已实现 |
| FR-025 | 模板元数据记录 | P0 | 已实现 |
| FR-026 | 模板修改 | P0 | 已实现 |
| FR-027 | 模板删除 | P0 | 已实现 |
| FR-028 | 模板重命名 | P0 | 已实现 |
| FR-029 | 模板搜索与排序 | P0 | 已实现 |
| FR-030 | 模板批量加载/卸载 | P0 | 已实现 |
| FR-031 | 模板测试识别 | P0 | 已实现 |
| FR-032 | 模板匹配推理 | P0 | 已实现 |
| FR-033 | 模板匹配结果择优 | P0 | 已实现 |
| FR-034 | 匹配失败处理 | P0 | 已实现 |
| FR-035 | 用户偏移量管理 | P0 | 已实现 |
| FR-036 | 相机曝光时间设置 | P0 | 已实现 |
| FR-037 | 相机增益设置 | P0 | 已实现 |
| FR-038 | 相机参数持久化 | P0 | 已实现 |
| FR-039 | 相机连接状态监控 | P0 | 已实现 |
| FR-040 | 触发源切换 | P0 | 已实现 |
| FR-041 | 四模式切换状态机 | P0 | 已实现 |
| FR-042 | 模式切换前置条件验证 | P0 | 已实现 |
| FR-043 | Idle 模式安全停止 | P0 | 已实现 |
| FR-044 | 模式切换信号通知 | P0 | 已实现 |
| FR-045 | 相机模式关联配置 | P0 | 已实现 |
| FR-046 | PLC 连接状态轮询 | P0 | 已实现 |
| FR-047 | 相机连接状态轮询 | P0 | 已实现 |
| FR-048 | 轮询间隔可配置 | P1 | 已实现 |
| FR-049 | 连接断开通知 | P0 | 已实现 |
| FR-050 | 启动失败通知 | P0 | 已实现 |

---

## 4. 启动与系统检查

### FR-001: MVS 进程冲突检测

**优先级**: P0  
**描述**: 启动时检测系统中是否已有 Hikrobot MVS 客户端（MVS.exe）运行。若存在冲突进程，阻止启动并提示用户关闭。

**实现细节**:
- 通过 `tasklist /FI "IMAGENAME eq MVS.exe"` 检测
- 检测超时: 3000ms
- 超时处理: 不阻塞启动（保守通行）
- 错误提示: "检测到 MVS 客户端正在运行，请先关闭"

**所属模块**: `app::PunchPressApp::checkMVSProcessConflict()`

---

### FR-002: 单实例锁

**优先级**: P0  
**描述**: 确保同一时间仅运行一个 PunchPressVision 实例，防止多点抢占相机资源。

**实现细节**:
- 使用 `QSystemSemaphore("PunchPressVision_InstanceLock")` 实现
- 初始信号量值: 1
- 错误提示: "系统已在运行，请勿重复启动"

**所属模块**: `app::PunchPressApp::checkSingleInstance()`

---

### FR-003: 配置文件完整性校验

**优先级**: P1  
**描述**: 启动时校验必要配置文件的存在与完整性。

**当前状态**: 桩代码（始终返回 true）。基础设施层在 build 时已加载配置（异常时回退到默认值），严格文件存在性检查待后续实现。

**所属模块**: `app::PunchPressApp::checkConfigIntegrity()`

---

### FR-004: 标定参数就绪性检查

**优先级**: P1  
**描述**: 启动时检查畸变标定、九点标定、双相机拼接标定参数的完整性。缺失参数仅记录警告日志，不阻塞启动。

**检查项**:
- `distortionReady`: 两相机内参是否已标定
- `ninePointReady`: HomMat2D 变换矩阵是否已计算
- `spliceReady`: 拼接映射图是否已生成

**所属模块**: `app::PunchPressApp::checkCalibReadiness()` → `global::CalibReadiness`

---

## 5. 运行模式

系统在 App 层支持 4 种运行模式，通过 `global::RunMode` 枚举定义。模式之间可互相切换，切换遵循状态机规则。

> **注意**：畸变标定、九点标定、双相机拼接三个功能已下沉至 infTool 层，不再作为 App 层的运行模式。相关标定操作通过独立工具程序完成（详见 [第 13 章](#13-独立工具程序)），但标定结果会被主应用的生产模式使用。

### 5.1 模式列表

| 枚举值 | 中文名 | 相机模式 | 帧率 | 触发源 |
|--------|--------|---------|------|--------|
| `Idle` | 空闲 | 软触发 | 3fps | Software |
| `Debug` | 调试 | 自由运行 | 3fps | — |
| `Production` | 工作 | 外部触发 | 30fps | Line0 |
| `CreateModel` | 创建模型 | 自由运行 | 3fps | — |

### 5.2 模式切换规则

#### FR-041: 四模式切换状态机

**描述**: 支持在 4 种模式间切换，切换为原子操作（`std::atomic<RunMode>`）。切换至 Idle 时停止相机取流，从 Idle 切换至其他模式时启动取流。

**前置条件**:
- 切换至 `Production`：必须已加载至少 1 个模板模型（`hasLoadedModel == true`），且建议完成所有标定（仅警告，不阻塞）

**所属模块**: `app::PunchPressApp::switchToMode()`

---

### 5.3 调试模式（FR-005 ~ FR-007）

#### FR-005: 自由运行采集

**描述**: 在调试模式下，两个相机均以自由运行模式工作，帧率 3fps。图像实时显示在 UI 主窗口。

#### FR-006: 实时画面显示

**描述**: 矫正/拼接后的图像通过 `HalconInteractiveLabel` 控件实时显示在主窗口左侧面板。

#### FR-007: 参数即时调节

**描述**: 在调试模式下可实时调节相机曝光、增益、光源开关，调节结果即时生效并持久化。

---

### 5.4 工作/生产模式（FR-008 ~ FR-010）

#### FR-008: 硬件触发采集

**描述**: 在工作模式下，两个相机切换为外部硬件触发模式（Line0），帧率上限 30fps。由生产线 PLC 或传感器触发采集。

#### FR-009: 前置条件检查

**描述**: 进入工作模式前必须验证：
- 标定参数完整性（畸变、九点、拼接）
- 至少加载 1 个模板模型（`hasLoadedModel == true`）
- 不满足则拒绝切换并返回错误信息

#### FR-010: 定位结果输出

**描述**: 每个触发帧执行多模型模板匹配，选取最高分结果，经九点标定变换为世界坐标，通过 Modbus 写入 PLC 寄存器。

**输出数据**:
- OffsetX (mm, 浮点)
- OffsetY (mm, 浮点)
- Angle (度, 浮点)
- Valid (bool)
- 匹配分数 Score
- 匹配到的模型 ID 与名称

**所属结构体**: `global::PositionResult`

---

### 5.5 创建模型模式

**描述**: 在创建模型模式下，两个相机以自由运行模式工作，帧率 3fps。图像实时刷新到 `ModelEditorDialog`，供用户绘制 ROI 并创建形状模板。

**所属模块**: UI: `ModelEditorDialog` → Business: `bun::ShapeModeManagerBun::createModel()`

---

## 6. 相机与控制

### 6.1 相机模块

#### FR-036: 曝光时间设置

**描述**: 支持独立设置两个相机的曝光时间（微秒级）。通过 UI 的数字键盘输入，即时生效，并持久化到 `cameraCfg`。

**默认值**: Camera1=200, Camera2=200

**所属模块**: `inf::CameraModule::setExposure()`

#### FR-037: 增益设置

**描述**: 支持独立设置两个相机的模拟增益。通过 UI 的数字键盘输入，即时生效，并持久化到 `cameraCfg`。

**默认值**: Camera1=1, Camera2=1

**所属模块**: `inf::CameraModule::setGain()`

#### FR-039: 相机连接状态监控

**描述**: 实时监控相机连接状态，连接/断开时通过信号链（CameraModule → CameraBun → PunchPressApp → UI）通知 UI 更新状态指示灯（绿色=已连接，红色=已断开）。

#### FR-040: 触发源切换

**描述**: 支持软件触发（Software）和硬件触发（Line0/Line1/Line2）的切换。不同运行模式使用不同触发源。
- Idle / Debug / CreateModel: 软件触发或自由运行（3fps）
- Production: 硬件触发 Line0（30fps）
- 标定工具（infTool 层）: 自由运行（1fps）

### 6.2 PLC 控制模块

#### PLC 通信

**描述**: 通过 Modbus TCP 与 PLC 通信，支持以下操作：
- 连接/断开（`connectToPLC` / `disconnectPLC`）
- 读/写单寄存器（16-bit）
- 读/写浮点数（2 个寄存器）
- 读/写多寄存器（批量）
- 读/写线圈

**默认连接**: IP=192.168.11.10, Port=502

**PLC 寄存器映射**（由 `PlcAddressCfg` 定义）:

| 寄存器地址 | 功能 | 数据类型 |
|-----------|------|---------|
| 40001 | OffsetX | Float (2 reg) |
| 40003 | OffsetY | Float (2 reg) |
| 40005 | Angle | Float (2 reg) |
| 40007 | Valid | Bool/Int |
| 40101 | NinePointArrived | Int |
| 40102 | NinePointIndex | Int |
| 40103+ | NinePointCoords | Float (每点 2 个，共 18 个) |

**所属模块**: `inf::ControlModule`

---

## 7. 光源控制

### FR-021: 上光源控制

**描述**: 支持开启/关闭上光源。通过 UI 单选按钮控制，状态变更时发出信号。

**所属模块**: `bun::LightControlBun::setUpperLight()` → `inf::LightIOModule`

### FR-022: 下光源控制

**描述**: 支持开启/关闭下光源。通过 UI 单选按钮控制，状态变更时发出信号。

**所属模块**: `bun::LightControlBun::setLowerLight()` → `inf::LightIOModule`

### FR-023: 光源状态查询

**描述**: 支持查询当前上下光源状态。创建模板时读取光源状态写入模型元数据。

**所属模块**: `bun::LightControlBun::getUpperLightState()` / `getLowerLightState()`

---

## 8. 视觉标定（infTool 层）

畸变标定、九点标定、双相机拼接三个视觉标定功能已下沉至 **infTool 层**。它们不通过主应用的运行模式切换来触发，而是：
- **运行时透明执行**：畸变矫正和双相机拼接在图像流管道中自动执行（`CalibInfTool` → `TwoCameraSpliceInfTool`），对上层透明
- **标定操作通过独立工具**：标定参数的计算与调试通过 3 个独立 GUI 工具程序完成（详见 [第 13 章](#13-独立工具程序)）
- **标定结果被主应用消费**：生产模式使用标定参数进行坐标变换和图像预处理

### 8.1 畸变标定（FR-011 ~ FR-014）

详见 [13.1 节 ToolCalibDistortion](#131-toolcalibdistortion)。

**核心算法流程**:
1. 使用 `CreateCalibData("calibration_object", 1, 1, ...)` 创建标定数据句柄
2. 设置初始相机参数（`area_scan_polynomial` 模型，焦距，2.4um 像素尺寸）
3. 对每个采集图像：灰度转换 → `FindCalibObject()` 检测标定板
4. `CalibrateCameras()` 执行内参优化
5. 提取相机参数（焦距、主点、畸变系数 K1-K3、P1-P2）和位姿
6. `SetOriginPose()` 应用板厚偏移
7. 持久化到 `CalibConfigItem`

**标定结果包含**:
- 相机内参 (cameraParameters): `area_scan_polynomial` 格式
- 相机外参 (cameraPose): 每个标定位置的位姿
- 标定误差 (calibrationErrors): 重投影 RMS 误差
- 参考图像索引 (calibrationReferenceIndex)

**所属模块**: `infTool::CalibInfTool::calibrateFromImages()`

### 8.2 实时畸变矫正

**描述**: 在图像流中实时应用畸变矫正（由 infTool 图像管道自动执行，对上层透明）：
- 读取已标定的相机参数
- 调用 `ChangeRadialDistortionCamPar("fixed", ...)` 生成无畸变参数
- 调用 `ChangeRadialDistortionImage()` 执行矫正
- 未标定或输入无效时原样返回（流水线安全降级）

**所属模块**: `infTool::CalibInfTool::undistortImage()`

### 8.3 九点标定（FR-015 ~ FR-018）

详见 [13.2 节 ToolNinePoint](#132-toolninepoint)。

#### FR-015: 前置条件检查

**描述**: 执行九点标定前检查畸变标定是否完成（仅警告，不阻塞）。

#### FR-016: 像素坐标采集

**描述**: 采集标定板在各位置的图像，通过 Halcon 模板匹配与测量检测标记点像素坐标。

**当前状态**: 自动检测部分待完善（`startAutoCalibration` 中的检测步骤标记为"待实现"）。

#### FR-017: PLC 协同自动标定

**描述**: 全自动九点标定工作流：
1. 在独立 `std::thread` 中运行
2. 逐点写入机械坐标到 PLC 寄存器
3. 写入当前点索引
4. 轮询 PLC 到位信号（`regNinePointArrived`），每 100ms 轮询一次，最多 300 次（30 秒超时）
5. 到位后采集图像并检测标记点
6. 完成所有 9 点后计算 HomMat2D 变换矩阵
7. 持久化到 `NinePointCfg`

**所属模块**: `infTool::NinePointInfTool::startAutoCalibration()`

#### FR-018: 像素到世界坐标变换

**描述**: 使用 `VectorToHomMat2d()` 计算最小二乘 HomMat2D，后续通过 `AffineTransPoint2d()` 将任意像素坐标转换为世界坐标。

**所属模块**: `infTool::NinePointInfTool::calcPixToWorldHomMat2D()` / `pixToWorld()`

**九点标定板参数**:
- 行点数 xnumber: 7
- 列点数 ynumber: 7
- 点间距 distance: 0.007m
- 缩放 scale: 0.5
- X 方向距离 xdiantance: 400
- Y 方向距离 ydistance: 100

### 8.4 双相机拼接（FR-019 ~ FR-020）

详见 [13.3 节 ToolTwoCameraSplice](#133-tooltwocamerasplice)。

#### FR-019: 拼接标定计算

**描述**: 使用 Halcon `CreateCalibData` / `FindCalibObject` / `ImagePointsToWorldPlane` 计算双相机的世界平面映射关系，生成 `MapSingle1` 和 `MapSingle2`。

**标定参数**:
- 标定板描述文件
- 重叠百分比（OverlapPercent, 默认 70%）
- 边界百分比（BorderPercent, 默认 7%）
- 标定板间距（DistancePlates）
- 像素当量（pixTowWorld）

**所属模块**: `infTool::TwoCameraSpliceInfTool::calibImage()`

#### FR-020: 运行时拼接

**描述**: 生产模式下，infTool 图像管道自动执行拼接（对上层透明）：
- 使用 `MapImage()` 校正两相机图像
- 使用 `TileImages()` 水平拼接
- 若几何拼接失败，降级为硬拼接（简单 ConcatObj + TileImages）
- 若仅单相机在线，直通该相机图像（500ms 超时兜底）

**所属模块**: `infTool::TwoCameraSpliceInfTool::onCalibFrame()` / `pinjieImage()`

---

## 9. 形状模板管理

### 9.1 模板数据模型

#### 模板元数据 (`Config::ShapeModelInfo`)
- UUID（唯一标识）
- 名称（用户命名）
- 创建时间 / 修改时间（yyMMddHHmmsszzz 格式）
- 文件夹路径

#### 模板数据 (`Config::ShapeModelData`)
- 模板匹配图像（_templateMatImage）
- 原始图像（_originalImage）
- 标注图像（_annotatedImage）
- Halcon 模型句柄（hv_ModelID, hv_MetrologyHandle）
- ROI 绘制列表（_paintCreateRoiList）
- 遮蔽区域列表（_paintShieldRoiList）
- XLD 轮廓显示对象
- 中心点坐标
- 偏移量（offsetX, offsetY, offsetAngle）
- 模型创建参数（曝光、增益、上下光源状态）
- 图像预处理参数（通道类型、开运算/闭运算核大小、均值滤波核大小）
- 训练参数（级别数、角度范围/步长、优化方法、度量方式、对比度、最小对比度）

#### 文件存储格式

| 文件 | 格式 | 内容 |
|------|------|------|
| `info.json` | JSON | 元数据（名称、ID、时间） |
| `template.bmp` | BMP | 模板匹配图像 |
| `original.bmp` | BMP | 原始采集图像 |
| `annotated.bmp` | BMP | 标注叠加图像 |
| `createObjs.hobj` | HOBJ | ROI 和遮蔽区域 |
| `model.shm` | SHM | Halcon Shape Model |
| `metrology.mmc` | MMC | Halcon Metrology Model |
| `model_params.txt` | TXT | 匹配参数（对比度、角度等） |
| `offset.txt` | TXT | 用户偏移量 |

### 9.2 模板 CRUD

#### FR-024: 模板创建

**描述**: 通过 `ModelEditorDialog` 创建新的形状匹配模板。

**流程**:
1. 切换到 `CreateModel` 模式，相机自由运行取流
2. 用户通过 ShapeEditor 绘制 ROI 区域和遮蔽区域
3. 可选：绘制中心点（手动定义匹配参考点）
4. 配置图像预处理参数（通道、开运算、闭运算、均值滤波）
5. 点击"识别"按钮执行 `testRecognize()` 预览匹配效果
6. 点击"创建模板"执行创建并持久化到磁盘

**所属模块**: `bun::ShapeModeManagerBun::createModel()`

#### FR-025: 模板元数据记录

**描述**: 创建模板时自动记录以下环境参数：
- 相机曝光时间（Camera1 / Camera2）
- 相机增益（Camera1 / Camera2）
- 上光源状态
- 下光源状态

#### FR-026: 模板修改

**描述**: 支持修改已有模板的 ROI、遮蔽区域、预处理参数、训练参数。修改时加载原始图像，相机停止。

**所属模块**: `bun::ShapeModeManagerBun::updateModel()`

#### FR-027: 模板删除

**描述**: 支持删除模板（含确认对话框）。删除操作不可逆。

**所属模块**: `bun::ShapeModeManagerBun::deleteModel()`

#### FR-028: 模板重命名

**描述**: 支持重命名模板。通过 FullKeyboard 输入新名称。

**所属模块**: `bun::ShapeModeManagerBun::renameModel()`

### 9.3 模板查询与加载

#### FR-029: 模板搜索与排序

**描述**: 在 `ModelManagerDialog` 中支持：
- 关键词搜索（名称模糊匹配）
- 排序方式：名称升序/降序、创建时间升序/降序
- 多选（Ctrl/Shift 扩展选择）

**所属模块**: `bun::ShapeModeManagerBun::searchModels()` / `getAllModels()`

#### FR-030: 模板批量加载/卸载

**描述**: 支持多选模板后批量加载到内存（Halcon 模型句柄），供生产匹配使用。部分加载失败不影响已加载的模型，返回失败列表。

**所属模块**: `bun::ShapeModeManagerBun::loadModels()` / `unloadModels()`

### 9.4 模板匹配

#### FR-031: 模板测试识别

**描述**: 在模型编辑器中，点击"识别"按钮调用 `testRecognize()`，临时创建模型并对当前帧执行匹配，预览匹配效果。

**所属模块**: `bun::ShapeModeManagerBun::testRecognize()`

#### FR-032: 模板匹配推理

**描述**: 在生产模式下，对每个触发帧遍历所有已加载模型执行 Halcon Shape Model 匹配。返回每个模型的最佳匹配结果。

**匹配顺序**: 按模型加载顺序依次匹配，最先匹配到的结果优先。

**所属模块**: `bun::ShapeModeManagerBun::match()`

#### FR-033: 匹配结果择优

**描述**: 从多模型匹配结果中选取最高分（score）的结果作为最终输出。若匹配到的坐标需经过用户偏移量修正。

**所属模块**: `app::PunchPressApp::processProductionFrame()`

#### FR-034: 匹配失败处理

**描述**: 当所有模型均未匹配到目标时：
- 发射 `positionResultReady` 信号，`valid=false`
- PLC 输出 valid=0

### 9.5 用户偏移

#### FR-035: 用户偏移量管理

**描述**: 支持为每个模型设置用户自定义偏移量（OffsetX, OffsetY, OffsetAngle），用于补偿机械装配误差。

**范围限制**:
- X/Y 偏移: ±9999.999 mm
- 角度偏移: ±360°

**持久化**: 偏移量保存到模型目录下的 `offset.txt`，加载模型时自动读取。

**所属模块**: `bun::ShapeModeManagerBun::getUserOffset()` / `setUserOffset()` → UI: `OffsetEditorDialog`

---

## 10. 生产检测

### 10.1 检测流水线

生产模式下每帧的处理流程（`processProductionFrame`）：

```
1. 接收拼接后图像
2. 遍历所有已加载模型执行模板匹配
3. 从匹配结果中选取最高分
4. 若匹配成功：
   a. 应用用户偏移量修正
   b. 通过九点标定 HomMat2D 将像素坐标转为世界坐标
   c. 发射 positionResultReady(valid=true)
   d. 通过 Modbus 写入 OffsetX, OffsetY, Angle, Valid 到 PLC
5. 若匹配失败：
   a. 发射 positionResultReady(valid=false)
   b. PLC 写入 Valid=0
6. 若九点标定未完成，降级为像素坐标直接输出（带偏移）
```

### 10.2 PLC 输出

生产检测结果写入的 PLC 寄存器地址（由 `PlcAddressCfg` 定义）：

| 寄存器 | 变量 | 说明 |
|--------|------|------|
| 40001-40002 | OffsetX | X 方向偏移 (float) |
| 40003-40004 | OffsetY | Y 方向偏移 (float) |
| 40005-40006 | Angle | 角度偏移 (float) |
| 40007 | Valid | 匹配有效标志 |

---

## 11. UI 交互

### 11.1 主窗口布局

主窗口 `PunchPress` 水平分割为左右两栏（2:1 比例）：

**左侧（图像显示区）**:
- `HalconInteractiveLabel` 控件：实时显示相机画面
- 支持滚轮缩放与拖拽平移

**右侧（控制面板）**:
- 标题栏：应用名称 + 退出按钮
- 状态行：Camera1 / Camera2 / PLC 连接状态指示灯（绿色=已连接，红色=已断开）
- 模式选择：调试模式 / 工作模式 互斥单选按钮
- 光源控制：上光源 / 下光源 独立开关按钮
- 相机参数：曝光1/增益1 / 曝光2/增益2 设置按钮（弹出数字键盘）
- 工具区：模型管理按钮
- 已加载模型列表：表格显示（模型名称 | OffsetX | OffsetY | OffsetAngle），双击行打开偏移编辑器
- 状态栏：显示当前定位结果或"未匹配"

### 11.2 对话框

#### ModelEditorDialog（模型编辑器）

- 尺寸: 1208×960
- 三列布局：图像显示 / 形状编辑 / 参数与操作
- 形状编辑工具：绘制 ROI（矩形/自由）、绘制遮蔽（矩形/自由）、中心点、清空、撤销
- 预处理配置：通道选择（灰度/R/G/B/H/S/V）、开运算、闭运算、均值滤波
- 操作按钮：识别（测试匹配）、创建/修改模板、读取图片、退出
- 两种模式：创建（实时帧）和修改（从磁盘加载图像）

#### ModelManagerDialog（模型管理器）

- 尺寸: 1600×900
- 水平分割：模型列表 / 模型详情
- 搜索栏：关键词搜索 + 清除按钮
- 排序：名称/创建时间 升序/降序
- 列表：支持多选（已加载模型蓝色加粗显示）
- 详情：模板预览图 + 元数据表（名称、通道、预处理、对比度、ROI/遮蔽数量、曝光/增益、创建时间、更新时间、文件夹路径）
- 操作：加载选中、全部卸载、重命名、删除、创建模型、修改模型
- 详细操作流程见 [第 9 章 形状模板管理](#9-形状模板管理)

#### OffsetEditorDialog（偏移编辑器）

- 程序化构建（无 .ui 文件）
- 三个输入项：OffsetX, OffsetY, OffsetAngle
- 使用数字键盘输入，带范围限制
- 保存后自动持久化

### 11.3 未实现的 UI 组件

| 组件 | .ui 文件 | 状态 |
|------|---------|------|
| `Dlg_OffSet` | `Dlg_OffSet.ui` | 存在 UI 文件，无对应 C++ 代码实现 |
| `LoadingDialog` | `LoadingDialog.ui` | 存在 UI 空模板，无对应 C++ 代码实现 |
| `tableWidget_info` | 内嵌于 PunchPress.ui | UI 控件存在，无代码填充 |
| `label_companyInfo` | 内嵌于 PunchPress.ui | UI 控件存在，无代码设置 |

---

## 12. 配置与持久化

### 12.1 配置层次

系统采用双层配置架构：

| 层次 | 技术 | 适用场景 |
|------|------|---------|
| OSO 序列化 | RWUL oso (XML 后端) | 简单结构化配置（IP、端口、曝光等） |
| 手写文件 IO | 自研（TXT/TUP/HOBJ/BMP/SHM/JSON） | 复杂视觉数据（标定参数、模型数据） |

### 12.2 OSO 配置项

#### baseCfg（baseCfg.xml）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| cameraIp1 | string | 192.168.11.1 | 相机 1 IP |
| cameraIp2 | string | 192.168.11.2 | 相机 2 IP |
| plcIp | string | 192.168.11.10 | PLC IP 地址 |
| plcPort | int | 502 | PLC Modbus 端口 |

#### cameraCfg（cameraCfg.xml）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| exposureTime1 | int | 200 | 相机 1 曝光时间 |
| exposureTime2 | int | 200 | 相机 2 曝光时间 |
| gain1 | int | 1 | 相机 1 增益 |
| gain2 | int | 1 | 相机 2 增益 |

#### plcAddressCfg（plcAddressCfg.xml）

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| regOffsetX | int | 40001 | X 偏移寄存器 |
| regOffsetY | int | 40003 | Y 偏移寄存器 |
| regAngle | int | 40005 | 角度寄存器 |
| regValid | int | 40007 | 有效标志寄存器 |
| regNinePointArrived | int | 40101 | 九点到位确认 |
| regNinePointIndex | int | 40102 | 九点索引 |
| regNinePointCoordsStart | int | 40103 | 九点坐标起始 |

#### visionCfg（visionCfg.json）

| 字段 | 类型 | 说明 |
|------|------|------|
| filterLeft | int | 图像裁切左边界 |
| filterTop | int | 图像裁切上边界 |
| filterRight | int | 图像裁切右边界 |
| filterBottom | int | 图像裁切下边界 |

### 12.3 手写文件 IO 配置项

#### CalibConfig

- 按相机索引存储 `CalibConfigItem`（相机内参、外参、标定误差）
- 存储路径: `{ProjectRoot}/CalibConfigModule/`
- 文件格式: `.tup`（Halcon HTuple）、`.txt`（键值对）

#### NinePointCfg

- HomMat2D 变换矩阵、标定板参数、测量参数
- 存储路径: `{ProjectRoot}/NinePointModule/`
- 文件格式: `.tup`（HomMat2D）、`.txt`（标定板参数）

#### TwoCameraSpliceCfg

- MapSingle1/MapSingle2 映射图、校正尺寸、拼接参数
- 存储路径: `{ProjectRoot}/TwoCameraSpliceModule/`
- 文件格式: `.hobj`（映射图）、`.txt`（参数键值对）

### 12.4 运行时数据路径

由 `global::GlobalPath.hpp` 定义，根路径为 `D:/zfkjData/PunchPressVision/`：

| 子目录 | 用途 |
|--------|------|
| `config/` | OSO 配置文件（baseCfg.xml, cameraCfg.xml, plcAddressCfg.xml, visionCfg.json） |
| `ShapeModels/` | 形状模板模型文件 |
| `calib/` | 标定相关文件（CalibConfigModule, NinePointModule, TwoCameraSpliceModule） |
| `logs/` | 运行日志 |

### 12.5 持久化安全

所有文件写操作遵循**临时文件 + 原子重命名**模式：
1. 写入 `.tmp` 临时文件
2. 成功后原子重命名为目标文件
3. 防止写入中断导致配置文件损坏

---

## 13. 独立工具程序

系统包含 3 个独立可执行的 GUI 工具程序，用于开发阶段的标定调试与算法验证。

### 13.1 ToolCalibDistortion

**路径**: `infTool/CalibInfTool/tool/ToolCalibDistortion/`  
**功能**: 交互式相机畸变标定与矫正工具

**主要功能**:
- 实时相机预览（双显示: 原始 / 矫正后对比）
- 相机选择（Camera1/Camera2）
- 曝光/增益控制
- 标定板 `.descr` 文件自动检测
- 采集标定图像（`drawCalibMarks` 验证标定板 → 仅保存有效图像）
- 标定流程（>= 3 张图像触发 `calibrateFromImages`）
- 角点预览（`CornerPreviewDialog`，XLD 十字叠加）
- 标定结果展示与自动保存

**标定结果显示**:
- 焦距 (mm)、像素尺寸 (um)
- 主点 (Cx, Cy)
- 径向畸变系数 (K1, K2, K3)
- 切向畸变系数 (P1, P2)
- 工作距离 (mm)、像素当量 (mm/px)
- 重投影误差 RMS (px)

### 13.2 ToolNinePoint

**路径**: `infTool/NinePointInfTool/tool/ToolNinePoint/`  
**功能**: 九点标定调试工具

**主要功能**:
- 双相机控制（独立启停、曝光、增益）
- 图像读取、ROI 绘制
- 形状模型创建与匹配
- 矩形查找（最多 3 个目标，用于九点标记检测）
- 九点数据表（像素 X/Y + 世界 X/Y 手动输入）
- Halcon 窗口缩放/拖拽
- 模板匹配 + 计量处理

### 13.3 ToolTwoCameraSplice

**路径**: `infTool/TwoCameraSpliceInfTool/tool/ToolTwoCameraSplice/`  
**功能**: 双相机拼接标定调试工具

**主要功能**:
- 三路 Halcon 显示（Camera1 / Camera2 / 拼接结果）
- 标定板描述文件路径初始化
- 焦距补偿、裁剪比例调节
- 材料管理（新建、重命名、删除）
- 拼接测试

---

## 14. 非功能需求

### 14.1 性能

| 指标 | 要求 |
|------|------|
| 生产模式帧率 | 30fps（硬件触发） |
| 调试/创建模型模式帧率 | 3fps（自由运行） |
| 标定工具帧率 | 1fps（自由运行，infTool 层） |
| 图像处理延迟 | 单帧处理 < 33ms（满足 30fps） |
| 拼接超时 | 500ms（等待配对帧超时则直通） |
| PLC 轮询间隔 | 100ms（九点标定到位检测） |
| PLC 到位超时 | 30s（30×100ms=3000 次轮询） |

### 14.2 可靠性

| 要求 | 说明 |
|------|------|
| 静默失败 | 生产设备稳定性优先，配置文件损坏时回退到默认值 |
| 异常安全 | 所有 Worker 线程循环必须捕获所有异常，防止线程崩溃 |
| 单实例运行 | 防止多实例抢占硬件资源 |
| 进程冲突检测 | 检测 MVS 客户端冲突 |
| 原子写入 | 配置文件写入使用临时文件 + 原子重命名 |
| 信号连接完整性 | 构造阶段完成所有信号连接，build 阶段发射初始状态，确保信号不丢失 |
| 顺序销毁 | 按构造逆序销毁模块，保证依赖关系正确 |

### 14.3 可维护性

| 要求 | 说明 |
|------|------|
| 五层严格分层 | 依赖方向始终向下，禁止跨层调用 |
| 聚合根模式 | 每层一个聚合根管理子模块生命周期 |
| 接口抽象 | global 层定义 IInfrastructure / IBusiness / IInfTool 接口 |
| 模块规范 | 每个模块遵循统一目录与 CMake 结构 |
| 命名空间隔离 | 应用层使用领域命名空间（app::、bun::、inf::），不污染 rw:: |
| CMake 别名 | 所有目标提供命名空间别名 |

### 14.4 兼容性

| 要求 | 说明 |
|------|------|
| 操作系统 | Windows 10 x64 或更高版本 |
| 编译器 | MSVC 2022（C++17） |
| Qt 版本 | Qt 6.7.3+ |
| Halcon 版本 | Halcon 24.11+ |
| OpenCV 版本 | OpenCV 4.12.0 |
| 相机兼容 | Hikrobot MVS 兼容相机（2 台） |

### 14.5 线程安全

| 原则 | 实施方式 |
|------|---------|
| 跨线程配置 | `std::atomic<RunMode>` + memory_order_acquire/release |
| 帧数据传输 | 图像入队前 clone/移动，出队后线程拥有所有权 |
| 信号连接类型 | 相机回调 DirectConnection（低延迟），跨线程 UI 更新 QueuedConnection |
| 线程分布 | 主线程(UI) + 相机回调线程 + 九点标定 Worker 线程 + 健康监控 Worker 线程 + Modbus 通信线程 |

---

## 15. 外部接口

### 15.1 硬件接口

| 接口 | 协议/设备 | 数量 | 说明 |
|------|----------|------|------|
| 相机 | Hikrobot MVS (GigE/USB3) | 2 | Camera1（左）/ Camera2（右） |
| PLC | Modbus TCP | 1 | 检测结果输出 + 九点标定协同 |
| 上光源 | 数字 IO（通过 PLC） | 1 | 照明控制 |
| 下光源 | 数字 IO（通过 PLC） | 1 | 照明控制 |

### 15.2 软件依赖

| 依赖 | 版本 | 用途 |
|------|------|------|
| Qt6 (Core/Widgets/Gui/Concurrent/Network) | 6.7.3 | UI 框架、线程、网络 |
| MVTec Halcon | 24.11 | 工业视觉算法引擎 |
| OpenCV | 4.12.0 | 图像格式转换（cv::Mat ↔ HImage） |
| RWUL | — | 配置序列化（oso）、相机通信（rqwcm）、UI 控件（rqwu）、Modbus（hoepModbus） |
| jsoncpp | 1.9.6 | JSON 处理 |
| spdlog | 1.17.0 | 日志 |
| libmodbus | 3.1.11 | Modbus 通信底层 |
| libzip | 1.11.4 | 压缩/解压 |
| sqlite3 | 3.51.2 | 数据库（预留） |
| pugixml | 1.15 | XML 处理 |

### 15.3 信号接口

系统跨层通信使用 Qt 信号/槽机制：

#### 图像帧信号链
```
CameraModule::callBackFunc(MatInfo, CameraIndex) [DirectConnection]
  → CalibInfTool::onCameraFrame (矫正)
    → CalibInfTool::callBackFunc(HImage, CameraIndex) [DirectConnection]
      → TwoCameraSpliceInfTool::onCalibFrame (拼接)
        → TwoCameraSpliceInfTool::callBackFunc(HImage) [DirectConnection]
          → CameraBun::callBackFunWithCalib(HImage) [QueuedConnection]
            → PunchPressApp::frameReady(HImage) [QueuedConnection]
              → UI::HalconInteractiveLabel
```

#### 状态信号链
```
CameraModule::cameraConnectionStateChanged
  → CameraBun::cameraConnectionStateChanged (转发)
    → PunchPressApp::cameraConnectionChanged (转发)
      → UI::onCameraConnectionChanged (状态指示灯)

ControlModule::connectionStateChanged
  → PunchPressApp::plcConnectionChanged (转发)
    → UI::onPlcConnectionChanged (状态指示灯)
```

---

## 16. 数据模型附录

### 16.1 核心枚举

```cpp
// 相机索引
enum class CameraIndex { Camera1, Camera2 };

// 运行模式（App 层 — 标定功能已下沉至 infTool 层）
enum class RunMode {
    Idle,              // 空闲
    Debug,             // 调试
    Production,        // 工作/生产
    CreateModel        // 创建模型
};

// 触发源
enum class TriggerSource { Software, Line0, Line1, Line2 };

// 光源状态
enum class LightState { Off, On };
```

### 16.2 核心数据结构

```cpp
// 定位结果
struct PositionResult {
    double offsetX, offsetY;    // 世界坐标偏移 (mm)
    double angle;               // 角度偏移 (度)
    double score;               // 匹配分数
    bool valid;                 // 有效标志
    int modelId;                // 模型 ID
    QString modelName;          // 模型名称
};

// 标定就绪性
struct CalibReadiness {
    bool distortionReady;       // 畸变标定完成
    bool ninePointReady;        // 九点标定完成
    bool spliceReady;           // 拼接标定完成
    bool allReady() const;
};

// 生产就绪性
struct ProductionReadiness {
    CalibReadiness calib;       // 标定状态
    bool hasLoadedModel;        // 已加载模板
    int loadedModelCount;       // 已加载模板数量
    bool plcConnected;          // PLC 已连接
    bool allReady() const;
};

// 匹配结果
struct MatchResult {
    int modelId;
    QString modelName;
    double row, column, angle;
    double score;
    double offsetX, offsetY;
    bool found;
};

// 创建模型请求
struct CreateModelRequest {
    HImage preprocessedImage;   // 预处理后图像
    HImage originalImage;       // 原始图像
    // ROI 与遮蔽区域
    // 中心点坐标
    // 曝光/增益/光源状态
    // 预处理参数
    // Halcon 训练参数
};

// 九点标定数据
struct Point2D { double x, y; };
struct PointPair { Point2D pixel, world; };
```

### 16.3 运行时数据路径

```
D:/zfkjData/PunchPressVision/
├── config/
│   ├── baseCfg.xml
│   ├── cameraCfg.xml
│   ├── plcAddressCfg.xml
│   └── visionCfg.json
├── ShapeModels/
│   ├── <UUID_1>/
│   │   ├── info.json
│   │   ├── template.bmp
│   │   ├── original.bmp
│   │   ├── annotated.bmp
│   │   ├── createObjs.hobj
│   │   ├── model.shm
│   │   ├── metrology.mmc
│   │   ├── model_params.txt
│   │   └── offset.txt
│   └── <UUID_2>/ ...
├── CalibConfigModule/
│   ├── Camera1/
│   │   ├── cameraParameters.tup
│   │   ├── cameraPose.tup
│   │   ├── ...
│   └── Camera2/ ...
├── NinePointModule/
│   ├── homMat2D.tup
│   └── config.txt
├── TwoCameraSpliceModule/
│   ├── MapSingle1.hobj
│   ├── MapSingle2.hobj
│   └── config.txt
└── logs/
```

---

> **文档维护**: 本文档基于项目源码（commit `4c91a5d`, branch `develop`）分析整理。  
> **相关文档**: [ARCHITECTURE.md](ARCHITECTURE.md)——架构风格指南；[CLAUDE.md](CLAUDE.md)——开发构建指南；[UI_DESIGN_LANGUAGE.md](UI_DESIGN_LANGUAGE.md)——UI 设计语言。
