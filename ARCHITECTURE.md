# RWUL 工业视觉检测系统架构风格指南

本文档描述基于 RWUL 平台构建工业视觉检测系统的通用架构设计语言与风格，适用于各类视觉质检、分拣、测量场景。

---

## 1. 核心理念

**分层解耦 + 流水线编排 + 声明式配置 + 依赖注入**

RWUL 提供工业视觉领域的基础设施抽象（图像处理流水线、AI 推理引擎、硬件通信、配置系统）。应用层在此基础上构建业务逻辑，遵循严格的分层架构和统一的生命周期管理。

**核心设计意图**：通过物理分层强制解耦。UI 不直接操作相机或读写配置，所有操作必须经过 App 层与 Business 层转译，使得 UI 可以被整体替换而不影响核心业务逻辑。Business 层通过构造函数接收 Infrastructure 层引用，实现依赖注入；Infrastructure 层使用显式组合控制构造与析构顺序。

---

## 2. 分层架构

所有基于 RWUL 的项目应采用以下**五层严格分层架构**，依赖方向始终向下，禁止跨层直接调用：

```
┌─────────────────────────────────────┐
│  UI 层 (Qt Widgets)                 │  ← 只依赖 App 层
├─────────────────────────────────────┤
│  App 层                             │  ← 只依赖 Business 层
│  应用服务（生命周期管理、跨模块协调）  │
├─────────────────────────────────────┤
│  Business 层 (Bundle)               │  ← 只依赖 Infrastructure 层
│  业务编排：检测流水线、设备控制、AI 训练 │
├─────────────────────────────────────┤
│  Infrastructure 层 (Module)         │  ← 只依赖 global 层
│  硬件/数据：相机、PLC、推理引擎、配置   │
├─────────────────────────────────────┤
│  global 层                          │  ← 不依赖任何其他层
│  共享类型、常量、版本、接口定义         │
└─────────────────────────────────────┘
```

**依赖规则**：只允许上层调用下层，禁止反向依赖。同一层内模块间通过接口或信号通信。

### 2.1 各层职责

| 层级 | 命名空间 | 目录约定 | 核心职责 |
|------|----------|---------|---------|
| UI | （无） | `UI/` | 用户界面、主窗口、对话框、模块生命周期 orchestrator（`Modules` 单例） |
| App | `app::` | `app/` | 应用服务容器（`Apps`），封装 Business + Infrastructure，提供 `IApp` 生命周期接口 |
| Business | `bun::` | `business/` | 领域逻辑：视觉检测流水线、设备控制状态机、AI 训练编排。每个基础设施能力对应一个业务编排单元（Bundle） |
| Infrastructure | `inf::` | `infrastructure/` | 硬件网关：相机模块、控制模块、AI 推理模块、配置模块、数据存储模块 |
| Global | `global::` | `global/` | 跨层共享的枚举、类型别名、路径常量、版本校验、接口定义（如 `IInfrastructure`、`IBusiness`） |
| Config | `Config::` | — | 配置数据模型（OSO 生成的配置类及手写配置结构体） |

### 2.2 层内聚合模式

每一层都有一个**聚合根（Aggregate Root）**负责按顺序构建、启动、停止子模块：

- `Modules`（UI 层单例）→ 持有所有 UI 模块和 `app::App`
- `Apps`（App 层）→ 持有所有 `IApp` 实现（检测服务、相机服务、控制服务等）
- `Business` / `bun::`（Business 层）→ 持有核心业务流程、设备控制、AI 训练。各 Bundle 与 Infrastructure 模块形成**一一映射**关系
- `inf::infrastructure`（Infrastructure 层）→ 使用 `std::unique_ptr` 组合所有模块，显式控制构造与析构顺序。持有相机、控制器、推理引擎、配置等硬件抽象

**镜像式映射关系**：

```
infrastructure/          business/
  CameraModule   ←────→   CameraBun
  ConfigModule   ←────→   CalibBun / CameraBun
  ...              ...      ...
```

每个基础设施能力对应一个业务编排单元，Bundle 负责将 Module 的原子能力组合成可执行的业务流程。

---

## 3. 生命周期管理

### 3.1 双接口体系

```cpp
// 通用模块接口（UI 层使用）
template<class BuildError = void>
class IModule {
    virtual BuildError build() = 0;
    virtual void destroy() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

// App 服务接口（继承 QObject，支持信号/槽）
class IApp : public QObject {
    virtual void build() = 0;
    virtual void destroy() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};
```

**调用顺序**：
```
build() → start() → ...运行中... → stop() → destroy()
```

**设计原则**：
- `build()` 只做一次性初始化（创建对象、连接信号、加载配置），不做耗时操作
- `start()` 开启运行时行为（启动线程、打开硬件连接、开始处理）
- `stop()` 优雅停止（停止线程、关闭连接）
- `destroy()` 释放资源（销毁对象、断开信号）。配置类在 `destroy()` 中调用 `save()`，确保配置不丢失

**关键设计细节**：
- `infrastructure` 析构时按**构造逆序**手动 `reset()` 各模块，避免依赖 `std::unique_ptr` 的默认析构顺序（其顺序与声明顺序相反，即与构造顺序一致，但显式 reset 更清晰地表达了设计意图）
- 所有模块的依赖关系在 `Business::build()` 和 `infrastructure::build()` 中集中装配，形成**集中式对象装配器**模式
- 反向顺序销毁：先启动的后停止，后构建的先销毁

### 3.2 容器实现模式

App 层容器按依赖顺序调用子模块：

```cpp
void Apps::build() {
    // 先构建基础设施依赖
    detectionApp->build();
    controlApp->build();
    cameraApp->build();
    // ...
    // 最后同步配置
    this->updateConfigFromInf();
}

void Apps::start() {
    // 按启动依赖顺序
    configApp->start();
    controlApp->start();
    cameraApp->start();
    detectionApp->start();
    // ...
}
```

---

### 3.3 构造 → build → start 三段式初始化

`build() → start()` 被进一步拆分为**构造 + build + start** 三个独立阶段，核心原则：**构造建立信号依赖图，build 初始化资源并发射状态，start 激活运行时。**

**初始化（正向顺序）**：

```
Phase 1: 构造     → 栈上声明所有对象，构造里只做 make_unique + connect 信号
                   各层聚合根在构造中递归创建子组件，形成完整的对象树和信号图
Phase 2: build    → infrastructure.build() → business.build() → app.build()
                   加载配置、连接相机、创建 Halcon 资源，emit 初始状态信号
Phase 3: start    → business.start() → app.start()
                   启动帧监控、连接 PLC、开启工作线程
Phase 4: show     → w.show() → a.exec()
                   UI 显示 + 事件循环
```

**清理（逆向顺序，先 stop 后 destroy）**：

```
Phase 5: stop     → app.stop() → business.stop()
                   逆序停止运行时（断开 PLC、停止帧流、停止线程）
Phase 6: destroy  → app.destroy() → business.destroy() → infrastructure.destroy()
                   逆序销毁资源（断开信号、释放硬件、保存配置）
```

**`main()` 模板**：

```cpp
int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    // Phase 1: 构造 — 只创建对象和连接信号
    inf::infrastructure infrastructure;
    bun::Business business(infrastructure);
    app::PunchPressApp app(business);
    ui::PunchPress w(app);

    // Phase 2: build — 初始化硬件、加载配置
    infrastructure.build();
    business.build();
    app.build();

    // Phase 3: start — 激活运行时
    business.start();
    app.start();

    // Phase 4: UI 显示
    w.show();
    const int code = a.exec();

    // Phase 5: stop — 逆序停止
    app.stop();
    business.stop();

    // Phase 6: destroy — 逆序销毁
    app.destroy();
    business.destroy();
    infrastructure.destroy();

    return code;
}
```

**聚合根构造与 build 职责分离示例**：

```cpp
// 构造：创建子对象（make_unique），不执行初始化逻辑
Business::Business(inf::infrastructure& inf)
    : inf_(inf)
{
    infTool_ = std::make_unique<infTool::infTool>(inf_);
    camera_bun = std::make_unique<CameraBun>(inf_, *infTool_);
    shape_mode_manager_bun = std::make_unique<ShapeModeManagerBun>(inf_, *infTool_);
    light_control_bun = std::make_unique<LightControlBun>(inf_, *infTool_);
    // 裸指针别名（从 infTool 子对象借出）
    calib_bun = infTool_->calib_bun.get();
    nine_point_bun = infTool_->nine_point_bun.get();
    two_camera_splice_bun = infTool_->two_camera_splice_bun.get();
}

// build：调用子组件 build，初始化资源、发射初始状态
void Business::build()
{
    if (infTool_) infTool_->build();
    if (camera_bun) camera_bun->build();
    if (shape_mode_manager_bun) shape_mode_manager_bun->build();
    if (light_control_bun) light_control_bun->build();
}
```

**设计意图**：

| 原则 | 说明 |
|------|------|
| **构造 ≠ 初始化** | 构造函数只分配内存和连接信号，不做可能失败的 IO/硬件操作 |
| **信号在构造中连接** | `QObject::connect` 必须在 `build()` 之前完成。`CameraModule::build()` 会立即 emit `cameraConnectionStateChanged`，若下游（`CameraBun` → `PunchPressApp` → UI）在 `build()` 中才 connect，初始状态信号将永久丢失 |
| **build 发射初始状态** | 硬件模块在 `build()` 中连接设备并 emit 当前状态，所有消费者已在构造中连线就绪，形成完整的 **构造连线 → build 发射 → 全链路接收** 时序 |
| **start 只激活** | 不创建资源，不连接信号，纯粹切换为"运行中"状态 |
| **stop/destroy 分离** | 先优雅停止（断开 PLC、停帧流），再释放资源（断开信号、保存配置、`reset()` 智能指针）。`infrastructure` 无 `start/stop`（纯被动层），故 stop 仅覆盖 `app` + `business` |
| **析构逆序 reset** | 聚合根析构函数中显式按构造逆序 `reset()` 各 `unique_ptr`，确保依赖关系正确析构（如 `light_control_bun` 依赖 `infTool_`，应先析构） |

### 3.4 信号桥接模式

跨层信号传递遵循**逐层桥接**原则：Infrastructure 发射原始信号，Business 层转发，App 层再转发，UI 层消费。每一层的构造函数中建立对下一层信号的连接，确保整条链路在 `build()` 启动前已贯通。

**相机连接状态信号链**：

```
CameraModule::cameraConnectionStateChanged          ← emit in build()
    ↓ connect (CameraBun 构造)
CameraBun::cameraConnectionStateChanged             ← 直接转发（信号→信号）
    ↓ connect (PunchPressApp 构造)
PunchPressApp::cameraConnectionChanged              ← 直接转发（信号→信号）
    ↓ connect (PunchPress 构造 → buildConnections)
PunchPress::onCameraConnectionChanged               ← UI 槽：更新 label 样式
```

**PLC 连接状态信号链**：

```
ControlModule::connectionStateChanged               ← emit in connectToPLC / disconnectPLC
    ↓ connect (PunchPressApp 构造)
PunchPressApp::plcConnectionChanged                 ← 直接转发（信号→信号）
    ↓ connect (PunchPress 构造 → buildConnections)
PunchPress::onPlcConnectionChanged                  ← UI 槽：更新 label 样式
```

**设计原则**：
- Business 层桥接 Infrastructure 信号，App 层桥接 Business 信号，UI 直接消费 App 信号
- 转发使用**信号直连**（`connect(sender, &Sender::signal, this, &ThisClass::signal)`），无需中间槽函数
- 帧流使用 `Qt::DirectConnection` 避免线程切换开销，状态通知使用 `Qt::QueuedConnection` 保证线程安全
- 各层 `destroy()` 中必须 `QObject::disconnect` 断开构造中建立的连接

---

## 4. 模块组织规范

### 4.1 镜像式目录结构

Infrastructure 模块与 Business Bundle 形成**一一映射**关系，每个模块遵循统一目录布局：

```
ModuleName/
├── CMakeLists.txt
├── include/
│   └── <namespace_path>/
│       └── ModuleName/
│           ├── ModuleName.hpp          ← 公共头文件
│           ├── Config/xxxConfig.hpp    ← 配置结构体
│           ├── ModuleNamePath.hpp      ← 路径常量
│           └── ModuleNameTypes.hpp     ← 类型定义
├── private/include/
│   └── 私有头文件.hpp                   ← 实现细节不对外暴露
├── private/src/
│   └── 私有实现.cpp
├── src/
│   ├── ModuleName.cpp                  ← 主实现
│   ├── Config/xxxConfig.cpp            ← 配置的序列化实现
│   ├── ModuleNamePath.cpp              ← 路径常量定义
│   └── ModuleNameTypes.cpp             ← 类型定义实现
└── test/
    ├── CMakeLists.txt
    ├── include/
    │   └── ModuleName.t.hpp            ← 测试头文件
    └── src/
        └── ModuleName.t.cpp            ← 测试主函数（独立可执行）
```

**设计意图**：每个基础设施能力对应一个业务编排单元，Bundle 负责将 Module 的原子能力组合成可执行的业务流程。公共接口与实现细节物理隔离，`private/` 目录确保实现不对外暴露。

### 4.2 命名空间映射规则

| 层级 | 命名空间 | CMake 别名 | 示例 |
|------|----------|-----------|------|
| global | `global::` | `global::global` | `global::CameraIndex` |
| App | `app::` | — | `app::DetectionApp` |
| Business | `bun::` | `bun::CameraBun` | `bun::CalibBun` |
| Infrastructure | `inf::` | `inf::CameraModule` | `inf::ConfigModule` |
| UI | （无） | `UI::UIModule` | `PunchPress`（主窗口） |
| Config | `Config::` | 无独立别名 | `Config::CalibConfig` |

### 4.3 CMake 规范

**命名约定**：
- 目标名：`BS::ModuleName`（应用项目）或 `RWUL::moduleName`（RWUL 项目）
- Debug 后缀：`d`（通过 `DEBUG_POSTFIX "d"` 设置）
- 使用 `add_library(BS::${BS_TARGET} ALIAS ${BS_TARGET})` 创建命名空间别名
- 使用 `add_library(inf::CameraModule ALIAS CameraModule)` 等，强制调用方使用命名空间限定

**源文件收集**：
```cmake
file(GLOB_RECURSE INCLUDE_LIST CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)
file(GLOB_RECURSE SRC_LIST CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(APPEND SRC_LIST ${INCLUDE_LIST})
```

使用 `CONFIGURE_DEPENDS` 支持增量构建时自动检测新文件，但 `GLOB_RECURSE` 也意味着 CMakeLists 不需要为每个新文件手动更新。

**链接顺序**：先 link 同层模块，再 link 下层模块，最后 link RWUL 和 Qt。

**add_subdirectory 顺序**：CMake 中 `add_subdirectory()` 的调用顺序应遵循**底层优先**原则——最底层的依赖模块放在前面，最顶层的放在后面，同级模块之间不强制要求顺序。

```cmake
# 正确：global → infrastructure → business → app → UI
add_subdirectory(global)
add_subdirectory(infrastructure)
add_subdirectory(Business)
add_subdirectory(app)
add_subdirectory(UI)
```

**依赖方向**：`UI → app → business → infrastructure → global`（上层依赖下层）。先 `add_subdirectory` 底层模块可确保其子目标在后续上层模块 `target_link_libraries` 时已经定义，符合 CMake 的依赖解析逻辑。

**构建系统**：
- **静态库优先**：所有模块编译为静态库（`STATIC`），最终由可执行文件链接
- **Debug/Release 隔离**：Debug 构建自动附加 `d` 后缀（如 `CameraModuled.lib`），避免 Release/Debug 库混淆

### 4.4 第三方库集成

| 库 | 集成方式 | 说明 |
|----|---------|------|
| Qt6 | `find_package` + `windeployqt` | 构建后自动部署 DLL |
| OpenCV | `find_package` | vcpkg 提供 |
| Halcon | 自定义 `INTERFACE IMPORTED` 目标 | 手动指定 include/lib/bin 路径 |
| RWUL | `find_package` + `list(APPEND CMAKE_PREFIX_PATH)` | 自定义 Find 模块或 Config 文件 |

---

## 5. 配置与持久化

### 5.1 双层配置架构

```
OSO 配置文件（文本）
    ↓ build 时 oso_wrap_oso() 生成 C++ 头文件
OSO 生成的强类型 C++ 类（cdm::BaseCfg、cdm::ProcessCfg 等）
    ↓ ConfigModule 加载并持有实例
ConfigModule（内存中的配置对象聚合体）
    ↓ 运行时转换为业务配置结构体
业务 Config 结构体（ProcessConfig、ActionConfig、NinePointCfg 等）
    ↓ 注入 Pipeline / 服务
运行时行为
```

### 5.2 OSO 序列化轨道（RWUL 框架）

用于**简单结构化配置**（如 `cameraCfg`、`baseCfg`）：
- 源文件为 `.oso` 格式，经 CMake 函数 `oso_wrap_oso()` 编译生成 C++ 头文件。
- 配置类需实现：
  - 从 `rw::oso::ObjectStoreAssembly` 的构造函数
  - `operator rw::oso::ObjectStoreAssembly()` 转换运算符
  - 拷贝构造、赋值、`==`/`!=`
- 运行时通过 `rw::oso::StorageContext`（Xml 后端）读写。

**特点**：强类型、编译期 schema 检查、适合频繁读写的运行时配置。

**OSO 文件示例**：
```oso
#include<string>

namespace cdm {
    class SystemCfg {
    public:
        cameraIp1 : std::string : default = "192.168.11.1";
        controllerPort : int : default = 502;
    };
}
```

### 5.3 手写文件 IO 轨道（自研实现）

用于**复杂视觉数据**（如 `CalibConfig`、`ShapeModelData`、`TwoCameraSpliceCfg`）：
- 每个配置类实现 `saveInDir(const std::string&)` 和 `loadInDir(const std::string&)`。
- 支持多种文件格式：
  - `.txt` — 键值对参数（如 `cameraExposure=10000`）
  - `.tup` — Halcon `HTuple` 原生序列化
  - `.hobj` — Halcon 对象序列化（`WriteObject`/`ReadObject`）
  - `.bmp` — 图像文件
  - `.shm` — Halcon Shape Model
  - `.mmc` — Halcon Metrology Model
  - `.json` — 结构化元数据（`ShapeModelInfo`）

**特点**：灵活处理二进制/视觉数据、目录即数据库、适合大对象和跨版本兼容。

### 5.4 配置加载模式

```cpp
// 1. 从 Infrastructure 读取 OSO 配置
auto& cfgModule = inf.configModule;

// 2. 构建业务配置（转换 + 默认值填充）
ProcessConfig ProcessConfig::loadConfig(infrastructure::Infrastructure& inf) {
    ProcessConfig result;
    result.pixelEquivalentMap[StationId::ST1] = cfgModule.cameraCfg.pixelEquivalent1;
    // ...
    return result;
}

// 3. 应用配置（原子替换 + dirty 标记）
void ImageProcess::setConfig(const ProcessConfig& cfg) {
    std::lock_guard<std::mutex> lk(_configMtx);
    processConfig_ = cfg;
    for (auto& lane : _lanes)
        lane.configDirty.store(true, std::memory_order_release);
}
```

**设计原则**：
- OSO 负责持久化存储和 UI 编辑；手写 IO 负责复杂视觉数据持久化
- 业务配置结构体负责运行时使用的格式和默认值
- 配置更新采用**标记-应用**模式，避免运行时加锁过深
- 所有配置字段必须有默认值，防止未初始化
- 标定算法直接读写配置对象，算法状态与配置状态共用同一内存对象，标定结果自动持久化

### 5.5 持久化安全哲学

所有文件写操作遵循统一的**原子替换模式**：
```cpp
void writeXxxSafe(const fs::path& filePath, ...) {
    fs::path tmp = filePath;
    tmp += ".tmp";           // 1. 写入临时文件
    // ... write to tmp ...
    replaceFile(tmp, filePath);  // 2. 原子重命名替换
}
```

**设计意图**：防止写入中途崩溃导致配置文件损坏，保证持久化操作的原子性。

---

## 6. 数据流与流水线模式

### 6.1 核心数据流

工业视觉检测的标准数据流：

```
相机采集 → 图像队列 → AI 推理 → 检测流水线 → 判决流水线 → 执行机构动作
   │           │          │            │            │
   │           │          │            │            └── ActionService
   │           │          │            └── JudgePipeline
   │           │          └── InspectionPipeline / SortingPipeline
   │           └── ImageProcessService (多线程队列)
   └── CameraModule::callback (信号)
```

### 6.2 信号链式传递

图像数据流呈现**信号链式传递**：
```
MVSCameraPassive::callBackFuncPost
  → CameraModule::callBackFunc (信号)
  → CameraBun::callBackFunc (槽，cv::Mat→HImage 转换)
  → CameraBun::callBackFunWithCalib (信号，带校准)
  → UI::onImageProcessingFrameReady (槽，Halcon 显示)
```

**设计意图**：利用 Qt 的信号槽机制实现**观察者模式**，使数据生产方（相机）与消费方（UI）完全解耦。`Qt::DirectConnection` 被显式指定，确保回调在同线程同步执行，避免图像帧的线程切换开销。

### 6.3 多通道并发模型

当系统需要同时处理多路输入（如多相机、多工位）时，采用 **Lane 模式**：

```cpp
struct Lane {
    StationId stationId;
    std::deque<FrameTask> q;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    std::atomic_bool stopRequested{ false };
    std::atomic_bool configDirty{ false };
    std::unique_ptr<business::DetPipeline> detPipeline;
    std::atomic_bool pipelineBuilt{ false };
    // 统计
    std::atomic_size_t drops{ 0 };
    std::atomic_size_t processed{ 0 };
};
std::array<Lane, N> _lanes;
```

**设计原则**：
- 每个通道独立线程 + 独立队列，避免互锁
- 队列容量限制，满时丢弃新帧（`drops` 计数）
- 配置热更新：标记 `configDirty`，worker 在下一轮处理前应用
- 图像数据在入队前 `clone()` 或移动，避免跨线程访问已释放缓冲区

### 6.4 RWUL FlowPro 流水线

业务层使用 `rw::flowpro::GraphPipeline` 构建 DAG 处理图：

```cpp
auto pipeline = rw::flowpro::GraphPipelineBuilder()
    .add(std::make_shared<ProcessorA>(), "ProcA")
    .add(std::make_shared<ProcessorB>(), "ProcB")
    .fromInput("detections").to("ProcA", "input")
    .from("ProcA", "output").to("ProcB", "input")
    .build();
```

**Pipeline 设计规范**：
1. 每个 Pipeline 封装为 `struct` + 纯静态方法
2. 输入通过 `context.setExternalInput(name, value)` 注入
3. 配置通过 `context.setUserConfig(shared_ptr)` 注入
4. 输出通过 `context.getProcessorOutput<T>(processorName, slotName)` 提取
5. 构建后必须调用 `validate()` 检查连接完整性

```cpp
struct InspectionPipeline {
    static rw::flowpro::GraphPipeline build();
    static void setInputMat(const cv::Mat& mat, rw::flowpro::ProcessingContext& context);
    static void setConfig(const std::shared_ptr<Config>& config, rw::flowpro::ProcessingContext& context);
    static std::optional<Result> getOutput(const rw::flowpro::ProcessingContext& context);
};
```

### 6.5 典型业务流程

**标定流程（CalibBun）**：
```
用户操作 → UI 对话框
  → bun::CalibBun::calibrateFromImages(himages)
    → Halcon CreateCalibData / FindCalibObject / CalibrateCameras
    → 结果写入 inf_.calib_config_module_->calibConfig
    → CalibConfig::saveInDir() 持久化到 .tup/.txt
```

**九点标定流程（NinePointBun）**：
```
像素坐标 + 世界坐标
  → calcPixToWorldHomMat2D() → VectorToHomMat2d (Halcon)
    → 生成 outHomMat2D → 保存到 NinePointCfg
  → pixToWorld() → AffineTransPoint2d (Halcon)
    → 输出世界坐标
```

**模板管理流程（ShapeModelManagerModule）**：
```
addShapeModelItem(data, baseInfo)
  → 生成 UUID + 时间戳目录
  → ShapeModelItem::saveInDir()
    → JSON 存元数据 + BMP 存图像 + HOBJ 存 ROI + SHM 存模型
```

---

## 7. 跨模块通信

### 7.1 通信方式选择

| 场景 | 方式 | 说明 |
|------|------|------|
| 同层/相邻层事件通知 | Qt 信号/槽 | 松耦合，线程安全（通过 ConnectionType 控制） |
| 高频数据流（图像帧） | 直接函数调用 + 队列 | 避免信号开销，如 CameraModule → ImageProcess |
| 配置/状态同步 | 直接引用访问 | 通过注入的 Infrastructure/Business 引用读取 |
| 异步硬件 IO | Future/Promise | 如控制器异步读写 |

### 7.2 信号/槽连接规范

```cpp
// 高频数据流：DirectConnection（同线程或已知安全）
QObject::connect(&cameraModule, &CameraModule::frameReady,
    &imgProcess, &ImgProcess::onFrameReady, Qt::DirectConnection);

// 跨线程通知：QueuedConnection（默认）
QObject::connect(&imgProcess, &ImgProcess::taskProcessed,
    &saveService, &SaveService::onTaskProcessed, Qt::QueuedConnection);

// 实时性要求高：DirectConnection
QObject::connect(&actionService, &ActionService::resultReady,
    this, &DetectionService::onResultReady, Qt::DirectConnection);
```

**原则**：
- 默认使用 `Qt::AutoConnection`
- 相机回调到图像处理使用 `DirectConnection`（减少延迟）
- 跨线程大量数据传递使用 `QueuedConnection`（避免阻塞）
- 信号作为数据流管道，使数据生产方与消费方完全解耦

---

## 8. 硬件抽象模式

### 8.1 网关模式

Infrastructure 层的每个硬件模块都是**网关（Gateway）**，封装第三方库的细节：

```cpp
// 相机网关
class CameraModule : public QObject {
    std::unordered_map<StationId, std::unique_ptr<rw::rqwc::MVSCameraPassive>> cameraMap;
signals:
    void frameReady(rw::rqwc::MatInfo matInfo, StationId station);
    void cameraConnectionStateChanged(global::CameraIndex, bool connected, QString reason);
};

// 控制网关
class ControlModule : public QObject {
    std::shared_ptr<rw::hoep::ModbusDevice> modbusDevice;
    std::unique_ptr<rw::hoep::ModbusDeviceScheduler> scheduler;
};
```

**设计原则**：
- 网关持有 RWUL 提供的底层对象，不直接暴露给上层
- 通过 Qt 信号向上层发送硬件事件（新帧、连接状态变化）
- 通过 `build(Config)` / `destroy()` 管理硬件连接生命周期
- 配置（IP、端口等）从 ConfigModule 获取，不硬编码

### 8.2 设备控制抽象

Business 层的设备控制采用**分层封装**：

```
DeviceControl（聚合根）
├── DeviceBase      → 设备基础状态（启停、接管）
├── DeviceMotion    → 运动控制（读位置、写执行位置）
├── DeviceIO        → IO 读写
├── DeviceIOMonitor → IO 状态监控
└── DeviceConfig    → 设备参数
```

**原则**：
- 上层使用领域术语（如 `actuateAtLocation`、`getCurrentPosition`），不直接操作寄存器地址
- 寄存器地址映射在 OSO 配置中定义
- 硬件操作封装为异步 Future，但业务层可根据需要阻塞等待（`.get()`）

---

## 9. 视觉算法集成模式

### 9.1 Halcon 作为算法核心

项目以 MVTec Halcon 为视觉算法主引擎，其使用模式呈现以下特征：
- **HTuple 作为主要数据载体**：相机参数、位姿、变换矩阵均以 `HTuple` 传递
- **HObject/HImage 作为图像载体**：所有图像处理链路使用 Halcon 原生对象
- **命令式 API 风格**：直接调用 Halcon C++ 算子（如 `CreateCalibData`、`FindCalibObject`），而非封装为高层类

### 9.2 OpenCV 作为辅助与桥接

OpenCV 仅在数据输入阶段使用：
- 相机采集返回 `cv::Mat`（通过 RWUL 的 `MVSCameraPassive`）
- `CameraImgConvert` 负责 `cv::Mat → HImage` 的格式转换
- 支持灰度（`CV_8UC1`/`CV_16UC1`）、BGR（`CV_8UC3`）、BGRA（`CV_8UC4`）

**设计意图**：OpenCV 负责生态兼容（相机驱动），Halcon 负责算法精度，两者通过显式转换层隔离。

### 9.3 算法与配置的耦合

标定算法（如 `CalibBun::calibrateFromImages`）直接读写 `inf_.calib_config_module_->calibConfig`，**算法状态与配置状态共用同一内存对象**。

**优点**：标定结果自动持久化，无需额外保存步骤。
**风险**：配置对象在算法执行期间被外部修改可能导致不一致。

---

## 10. AI 推理抽象

### 10.1 引擎池模式

```cpp
class AiInferenceModule {
    std::unique_ptr<rw::ime::ModelEnginePool> baseEnginePool_;  // 基础检测模型
    std::unordered_map<StationId, std::unique_ptr<rw::ime::ModelEngine>> stationEngineMap_;  // 各通道专用模型
};
```

**推理调用模式**：
```cpp
// 主推理（基础模型）
auto engine = inf.aiInferenceModule.baseEnginePool_->acquire();
detResult = engine->processImg(mat);

// 分支推理（并发执行）
auto branchFuture = QtConcurrent::run(QThreadPool::globalInstance(), [mat, &detResultSort, ...]() {
    auto& engine = inf.aiInferenceModule.getStationEngine(task.stationId);
    detResultSort = engine->processImg(mat);
});
branchFuture.waitForFinished();
```

**原则**：
- 使用引擎池管理昂贵的模型实例（避免重复创建/销毁）
- 多模型并发推理使用 `QtConcurrent::run` 在线程池中并行
- 推理结果统一使用 `rw::imgpro::detection::DetResult` 类型

---

## 11. 并发与线程模型

### 11.1 线程分布

| 线程 | 来源 | 职责 |
|------|------|------|
| 主线程 | Qt `QApplication` | UI 事件循环、模块生命周期管理 |
| 相机回调线程 | `rw::rqwc::MVSCameraPassive` | 图像采集，通过信号触发处理 |
| 图像处理 Worker × N | `std::thread` | 每通道一个，处理图像队列 |
| 动作执行 Worker | `std::thread` | 单线程，处理执行机构任务队列 |
| 硬件通信线程 | `rw::hoep::ModbusDeviceScheduler` | 异步总线读写 |
| AI 推理线程 | `QtConcurrent::run` | 线程池中的推理任务 |

### 11.2 线程安全原则

1. **数据所有权**：每帧图像在进入队列前完成数据拷贝（`clone()` 或移动构造），出队后该线程拥有数据所有权
2. **配置访问**：读配置不加锁（原子量或拷贝），写配置使用互斥锁 + dirty 标记
3. **状态同步**：跨线程状态使用 `std::atomic` + memory order（`acquire/release`）
4. **异常安全**：Worker 循环必须捕获所有异常，防止线程崩溃

```cpp
void ImageProcess::workerLoop(StationId station) {
    for (;;) {
        FrameTask task;
        { // 等待并取出任务
            std::unique_lock<std::mutex> lk(lane.mtx);
            lane.cv.wait([&] { return stopRequested || !lane.q.empty(); });
            if (stopRequested) break;
            task = std::move(lane.q.front()); lane.q.pop_front();
        }
        try {
            processTask(task, *lane.detPipeline);
        } catch (const std::exception&) {
            // 记录日志，不退出线程
        } catch (...) {
            // 记录日志，不退出线程
        }
    }
}
```

---

## 12. 错误处理设计哲学

### 12.1 静默失败优先（Fail-Silent Philosophy）

项目中存在大量 `catch (...)` 且内部为空的代码块：
```cpp
try {
    // 加载配置 / 保存模型 / 删除文件
} catch (...) {
    // Ignore errors
}
```

**设计意图**：
- 视觉应用作为**生产设备软件**，崩溃的代价远高于功能降级
- 配置文件损坏时回退到默认值，确保设备能继续运行
- 图像加载失败时返回空对象，由上层决定是否显示错误提示

### 12.2 错误信息输出参数模式

算法函数采用 `std::string* errorMsg = nullptr` 参数传递错误信息：
```cpp
bool calibrateFromImages(..., std::string* errorMsg = nullptr);
```

**设计意图**：
- 调用方可选择忽略错误详情（传 `nullptr`）
- 需要时通过指针获取中文错误描述，便于 UI 层直接展示
- 保持返回值为 `bool` 的简洁语义（成功/失败）

### 12.3 Halcon 异常处理

Halcon API 调用统一包裹在 `try/catch (HalconCpp::HException&)` 中，并执行：
- 资源清理（如 `ClearCalibData`）
- 错误信息提取（`except.ErrorMessage().Text()`）
- 优雅返回 `false`

---


## 13. 命名规范

### 13.1 RWUL 命名空间

```
rw::ime        → AI 推理引擎核心（ModelEngine、DetectionRectangleInfo）
rw::imet       → 推理引擎工厂（TensorRT 实现）
rw::imgpro     → 图像处理（DetResult、ProcessorFilter、GraphPipelineBuilder）
rw::flowpro    → 流水线框架（GraphPipeline、ProcessingContext）
rw::oso        → 配置系统（StorageContext、oso_wrap_oso）
rw::hoep       → 工业总线通信（ModbusDevice、ModbusDeviceScheduler）
rw::rqwc       → 相机抽象（MVSCameraPassive、MatInfo）
rw::rqw        → UI 工具组件（MessageBox、LabelWarning、LoadingDialog）
rw::rqwu       → 通用工具（ThreadPool、RunEnvCheck）
rw::core       → 基础几何类型（PointPixel、RectanglePixel）
rw::imev       → 视觉事件类型（DetRect、ClassId）
rw::lgm        → 日志系统
```

### 13.2 应用层命名规范

```
命名空间：
  global::       → global 层共享类型与常量
  app::          → App 层服务
  bun::          → Business 层逻辑
  inf::          → Infrastructure 层硬件抽象
  Config::       → Config Data Model（配置类）

类名：
  PascalCase，反映领域概念
  如：DetectionService、ImageProcess、DeviceMotion、ConfigModule、CameraBun、CalibBun

接口名：
  I 前缀：IApp、IModule、IInfrastructure、IBusiness

文件名：
  与主类同名，如 DetectionService.hpp / DetectionService.cpp
  测试：Test_模块名 或 ModuleName.t.cpp，如 Test_DetectionService

成员变量：
  snake_case + 下划线后缀，如 camera_module_、shape_model_infos_、config_
  原子量：说明语义，如 stop_requested_、running_

信号/槽：
  on + 事件名（槽）：onFrameReady、onTaskProcessed
  动词 + 事件名（信号）：taskProcessed、resultReady
```

---

## 14. 测试模式

### 14.1 独立可执行测试

不使用 Google Test 等框架，测试是小型 Qt 应用程序：

```cpp
int main() {
    QApplication a(__argc, __argv);
    inf::infrastructureProvider::shared()->build();
    bun::Business business(*inf::infrastructureProvider::shared());
    business.build();
    // ... 执行被测逻辑 ...
    return a.exec();
}
```

**原则**：
- 每个模块的 `test/` 目录包含独立可执行文件
- 测试构造真实依赖（Infrastructure + Business），不 Mock（除非测试纯算法）
- 测试目标名：`Test_模块名` 或 `Test_父模块_子模块`，或 `ModuleName.t.cpp`

---

## 15. 可复用的架构原则清单

### 新建基于 RWUL 的项目时，应遵循：

1. **采用五层架构**：UI → App → Business → Infrastructure → Global
2. **实现生命周期接口**：所有服务实现 `build/destroy/start/stop`
3. **使用聚合根**：每层一个容器类管理该层所有模块的生命周期
4. **硬件抽象层**：所有硬件访问通过 Infrastructure 网关，禁止上层直接调用 RWUL 硬件 API
5. **配置双轨架构**：OSO 持久化 + 手写文件 IO → 业务配置结构体 → 运行时注入
6. **图像处理流水线**：使用 `rw::flowpro::GraphPipeline` 编排检测/绘制/判决逻辑
7. **多 Lane 并发**：多路输入时为每路分配独立线程 + 队列
8. **引擎池管理**：AI 模型使用 `rw::ime::ModelEnginePool` 管理实例生命周期
9. **信号/槽通信**：跨模块事件通知使用 Qt 信号，高频数据流使用直接调用
10. **静默失败优先**：生产设备稳定性优先，宁可功能降级也不允许崩溃
11. **异常安全**：所有 worker 线程循环捕获异常，防止单点故障导致系统崩溃
12. **命名空间隔离**：应用层使用领域命名空间（`app::`、`bun::`、`inf::`），不污染 `rw::`
13. **CMake 别名**：所有目标提供命名空间别名（`BS::TargetName`、`inf::TargetName`）
14. **原子写入**：所有配置文件写操作遵循临时文件 + 原子重命名替换模式
15. **Halcon/OpenCV 分工**：OpenCV 负责相机兼容与格式转换，Halcon 负责算法精度

---

## 16. 附录：RWUL 核心类型速查

| 领域概念 | RWUL 类型 | 说明 |
|---------|-----------|------|
| 检测结果 | `rw::imgpro::detection::DetResult` | 检测框集合 |
| 检测矩形 | `rw::ime::DetectionRectangleInfo` | 含角度、分数、掩膜的检测框 |
| 像素坐标 | `PointPixel`、`RectanglePixel` | 图像坐标系 |
| 图像帧 | `rw::rqwc::MatInfo` | 包装 `cv::Mat` 的帧信息 |
| 类 ID | `rw::imev::ClassId` / `rw::ime::ClassId` | 缺陷/目标类别标识 |
| 流水线 | `rw::flowpro::GraphPipeline` | DAG 处理图 |
| 上下文 | `rw::flowpro::ProcessingContext` | 流水线输入/输出/配置载体 |
| 推理引擎 | `rw::ime::ModelEngine` | 抽象推理接口 |
| 引擎工厂 | `rw::imet::ModelEngineFactory` | TensorRT 引擎创建 |
| 配置存储 | `rw::oso::StorageContext` | OSO 配置读写 |
| 工业总线 | `rw::hoep::ModbusDevice` | Modbus 设备通信 |
| 相机被动采集 | `rw::rqwc::MVSCameraPassive` | 回调式相机采集 |
