设计语言文档


---

## 1. 整体架构布局 (Layout Strategy)

### 主窗口: `QMainWindow`
- 窗口尺寸: 2047×1213（`ButtonScanner.ui`），适配大屏工业设备
- 中央 Widget 使用 **QHBoxLayout** 作为顶层布局，默认左右分栏（stretch 比例约 2:1）
- 左侧为图像显示区，右侧为控制面板区

### 对话框: `QDialog`
- 所有子对话框统一使用 `QDialog` 基类
- 大对话框: 1600×900 / 1717×1044 / 1698×1063
- 小对话框: 600×196 / 458×176

### 通用布局原则
- **左侧图像 + 右侧控制** 为核心范式（DlgCreateFeatureBank、DlgNewProduction tab_A/B/C、DlgSetBasePara 均采用）
- 右侧面板使用 `QVBoxLayout` 垂直堆叠，配合 `stretch` 控制间距比例
- `QSpacer` 大量使用以控制空白区域
- 图像区使用 `QGridLayout` 配合 `QLine`（分隔线）实现 2×2 四工位网格

---

## 2. 配色体系 (Color Palette)

### 背景色系统

| 用途 | 颜色值 | 使用场景 |
|------|--------|---------|
| 主背景浅灰 | `#F8F8F8` | 图像显示区 GroupBox、通用容器 |
| 中灰背景 | `rgb(181, 181, 181)` | 主界面右侧控制器 GroupBox |
| 深灰背景 | `rgb(202, 202, 202)` | 对话框右侧控制面板 |
| 第三灰背景 | `rgb(195, 195, 195)` | DlgSetBasePara 嵌套 GroupBox |
| 暗灰标题栏 | `rgb(81, 81, 81)` | 主界面顶部标题区域 |
| 纯白背景 | `rgb(255, 255, 255)` | 按钮、进度条、数据标签 |
| 浅灰 | `rgb(157, 157, 157)` | TabWidget pane |
| 浅灰 | `rgb(161, 161, 161)` | DlgRealTimeImgDis 底部操作区 |

### 文字色系统

| 用途 | 颜色值 | 使用场景 |
|------|--------|---------|
| 标题文字（暗底上） | `rgb(255, 255, 255)` | 主窗口顶部标题栏 |
| 标题文字（亮底上） | `rgb(89, 89, 89)` | 对话框标题 |
| 副标题/标签 | `rgb(80, 80, 80)` | 控制区标签 |
| 状态标签 | `rgb(85, 85, 85)` | 相机/板卡状态 |
| 主要正文 | `#444444` / `#333` | 通用文本 |
| 次级文字 | `#666666` | QGroupBox 通用 |
| 高亮数据值 | `rgb(0, 0, 0)` | 生产总量等关键数据 |
| 吹气数量强调 | `rgb(244, 0, 122)` | A/B 斗吹气计数 |
| 选中状态文字 | `#2196F3` | RadioButton/CheckBox 选中 |

### 边框与分隔色

| 用途 | 颜色值 |
|------|--------|
| 常规边框 | `#DDD` |
| QGroupBox 边框（控制区） | `#e0e0e0` |
| 按钮边框 | `#CCC` |
| 标签下划线分隔 | `#cccccc` |
| 悬停边框 | `#999` → `#666` |

### 控件状态色

| 状态 | 颜色值 | 语义 |
|------|--------|------|
| 🔵 蓝色 | `#2196F3` | 模式选择（调试/采图/剔除） |
| 🔵 深蓝 | `#1565C0` | 蓝色控件 pressed 态 |
| 🔵 浅蓝渐变 | `#BBDEFB` | 蓝色控件 pressed 内圆 |
| 🟡 黄色 | `#FFD700` | 光源选择 / 工位选择 |
| 🟡 深黄 | `#CC9900` | 黄色控件 pressed 边框 |
| 🟡 浅黄渐变 | `#FFF59D` | 黄色控件 pressed 内圆 |
| 🟠 橙色 | `#FF6B00` | 缺陷/正反/回料/色差/刀型 |
| 🟠 深橙 | `#CC5500` | 橙色控件 pressed 边框 |
| 🟠 浅橙渐变 | `#FFD8B3` | 橙色控件 pressed 内圆 |
| 🟤 橙红 | `rgb(255, 152, 83)` | CheckBox 选中（显示切换） |
| 🟤 暗红 | `darkred` | CheckBox 选中 hover |
| 🟢 成功绿 | `#228B22` | 状态标签 success |
| 🟠 警告橙 | `#FF8C00` | 状态标签 warning |
| 🔴 错误红 | `#B22222` | 状态标签 error |
| 🔴 亮红 | `#cc0000` / `#ff6666` | 退出/关闭按钮 hover/pressed |

---

## 3. 组件规范 (Component Specifications)

### 3.1 QGroupBox — 最核心的容器组件

统一使用 **两种语义变体**，背景色区分内容区与控制区：

#### 变体 A — 内容/图像显示容器（浅色）

```css
QGroupBox {
    border: 1px solid #DDD;
    border-radius: 4px;
    font: bold 14px;
    color: #666;
    background-color: #F8F8F8;
    padding: 5px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 5px;
}
```

**使用场景**: 图像预览区域、Tab 内图像区、主信息容器、报警管理主区域

#### 变体 B — 控制面板容器（中灰圆角）

```css
QGroupBox {
    font-size: 20px;
    font-weight: bold;
    background-color: rgb(181, 181, 181);
    border: 1px solid #e0e0e0;
    border-radius: 15px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 5px;
    color: #2c3e50;
}
```

**使用场景**: 主界面右侧控制面板区域、对话框操作面板（所有右侧控制区统一为该风格）

> **规范要点**: 所有控制面板 GroupBox 统一使用 `rgb(181, 181, 181)` 背景 + `15px` 大圆角，不再混用 `rgb(202,202,202)` / `rgb(195,195,195)` 等灰色。

---

### 3.2 QPushButton — 按钮

#### 标准按钮（主要风格，占 90%+）

```css
QPushButton {
    padding: 6px 14px;
    border: 2px solid #CCC;
    border-radius: 4px;
    background-color: white;
    color: #444;
    font-size: 18px;   /* 字号因场景不同使用 14/15/18/20/23/30px */
}
QPushButton:hover {
    border-color: #999;
    background-color: #F5F5F5;
}
QPushButton:pressed {
    border-color: #777;
    background-color: #EEE;
}
```

#### 特殊按钮变体

**① 图标按钮（最小化/关闭 — 标题栏专用）**
```css
QPushButton {
    color: white;
    border: none;
    border-radius: 5px;
    padding: 8px 16px 8px 36px;
    font-size: 14px;
    qproperty-icon: url(:/ButtonScanner/image/min.png);  /* 或 close.png */
    qproperty-iconSize: 30px 30px;
}
QPushButton:pressed { background-color: #cc0000; }
```

**② 深色操作按钮（产量清零）**
```css
QPushButton {
    padding: 6px 14px;
    border: 2px solid #CCC;
    border-radius: 4px;
    background-color: rgb(79, 79, 79);
    color: rgb(255, 255, 255);
    font-size: 15px;
}
```

**③ 图标退出按钮（DlgExposureTimeSet）**
```css
QPushButton {
    padding: 8px 16px 8px 36px;
    font-size: 14px;
    qproperty-icon: url(:/ButtonScanner/image/exit.png);
    qproperty-iconSize: 30px 30px;
}
QPushButton:hover  { background-color: #ff6666; }
QPushButton:pressed { background-color: #cc0000; }
```

#### 字号等级（统一 4 级）

| 等级 | 字号 | 使用场景 |
|------|------|---------|
| 超大 | 30px | 对话框中主要操作按钮 |
| 大 | 23px | 报警管理中的数值按钮、参数设置等强调型按钮 |
| 标准 | 18px | 绝大部分按钮（主界面控制区、对话框操作区） |
| 小 | 14px | 标题栏图标按钮、辅助操作按钮 |

> **规范要点**: 字号精简为 4 级，不再使用 15/17/20/25/35px 的散落字号。若需要中间尺寸，优先对齐到最近的标准级。

---

### 3.3 QRadioButton — 单选按钮

#### 统一结构

```css
QRadioButton {
    spacing: 8px;
    font-size: 18px;
    color: #333;
    font-weight: bold;
}
QRadioButton::indicator {
    width: 20px;
    height: 20px;
    border-radius: 10px;
    border: 2px solid #999;
    background: #ffffff;
}
QRadioButton::indicator:hover { border: 2px solid #666; }
QRadioButton::indicator:disabled {
    border: 2px solid #ccc;
    background: #eee;
}
/* 选中状态 — 按主题色变化，见下表 */
QRadioButton::indicator:checked {
    border: 2px solid <主题色>;
    background-color: qradialgradient(
        cx:0.5, cy:0.5, radius:0.4,
        fx:0.5, fy:0.5,
        stop:0 <主题色>,
        stop:1 transparent
    );
}
/* pressed 动画 */
QRadioButton::indicator:pressed {
    border: 2px solid <深色>;
    background-color: qradialgradient(
        cx:0.5, cy:0.5, radius:0.5,
        fx:0.5, fy:0.5,
        stop:0 <浅色>,
        stop:1 transparent
    );
}
```

> **规范要点**: QRadioButton 基础块只保留 `color: #333; font-weight: bold;`，不再出现 `color: rgb(0,0,0)` 的重复覆盖。所有 `:::disabled` 已修正为 `::indicator:disabled`。

#### 四种主题色语义对照表

| 主题 | 边框/内圆色 | Pressed 边框 | Pressed 内圆 | 语义 | 使用文件 |
|------|------------|-------------|-------------|------|---------|
| 🔵 蓝色 | `#2196F3` | `#1565C0` | `#BBDEFB` | 调试模式 / 采图 / 剔除功能 | ButtonScanner |
| 🟡 黄色 | `#FFD700` | `#CC9900` | `#FFF59D` | 光源选择 / 工位选择 | ButtonScanner, DlgCreateFeatureBank, DlgSetBasePara, DlgNewProduction |
| 🟠 橙色 | `#FF6B00` | `#CC5500` | `#FFD8B3` | 缺陷/正反/回料/色差/刀型 | ButtonScanner, DlgNewProduction |
| ⚪ 灰色 | `#999` | — | — | 未选中默认态 | 所有文件 |

---

### 3.4 QCheckBox — 复选框

统一采用 **方形蓝色** 风格（与 QRadioButton 蓝色主题保持一致）：

```css
QCheckBox {
    spacing: 8px;
    font-size: 18px;
    color: #333;
    font-weight: bold;
}
QCheckBox::indicator {
    width: 20px;
    height: 20px;
    border-radius: 4px;
    border: 2px solid #999;
    background: #ffffff;
}
QCheckBox::indicator:hover   { border: 2px solid #666; }
QCheckBox::indicator:checked {
    border: 2px solid #2196F3;
    background-color: qradialgradient(
        cx:0.5, cy:0.5, radius:0.4,
        fx:0.5, fy:0.5,
        stop:0 #2196F3,
        stop:1 transparent
    );
}
QCheckBox:checked { color: #2196F3; font-weight: 500; }
QCheckBox::indicator:disabled { border: 2px solid #ccc; background: #eee; }
QCheckBox::indicator:pressed {
    border: 2px solid #1565C0;
    background-color: qradialgradient(
        cx:0.5, cy:0.5, radius:0.5,
        fx:0.5, fy:0.5,
        stop:0 #BBDEFB,
        stop:1 transparent
    );
}
```

> **规范要点**: 全项目统一为上述蓝色风格，不再使用之前的橙红风格（`rgb(255,152,83)` / `darkred`）。选中态、悬停态、pressed 态均与 QRadioButton 蓝色主题完全一致。

---

### 3.5 QLabel — 标签

#### 标题型标签（带下划线分隔）

```css
QLabel {
    font-size: 18px;
    font-weight: bold;
    color: rgb(85, 85, 85);
    padding: 5px 5px;
    border-bottom: 2px solid #cccccc;
}
```

**使用场景**: 对话框标题、区域小标题、步骤标题

> **规范要点**: 统一使用 `color: rgb(85, 85, 85)`，不再混用 `rgb(89,89,89)` / `rgb(80,80,80)`。

#### 通用标签样式表（应在项目级 `.qss` 中统一定义）

该样式表定义了一套完整的 QLabel 语义类，应在项目中作为公共资源引用，避免在每个 `.ui` 文件中内联重复：

```css
/* 基础标签样式 */
QLabel {
    color: #444444;
    font-size: 18px;
    background: transparent;
    padding: 2px 4px;
}
/* 标题样式 */
QLabel.title {
    font-size: 16px;
    font-weight: 600;
    color: #333333;
    border-bottom: 1px solid #DDDDDD;
    padding-bottom: 4px;
}
/* 副标题/次要文本 */
QLabel.subtitle {
    color: #666666;
    font-size: 14px;
    font-style: italic;
}
/* 强调文本 */
QLabel.emphasis {
    color: #333333;
    font-weight: 500;
    background-color: #F5F5F5;
    border-radius: 3px;
    padding: 4px 8px;
}
/* 状态标签（成功/警告/错误） */
QLabel[state="success"] { color: #228B22; border-left: 3px solid #228B22; padding-left: 8px; }
QLabel[state="warning"] { color: #FF8C00; border-left: 3px solid #FF8C00; padding-left: 8px; }
QLabel[state="error"]   { color: #B22222; border-left: 3px solid #B22222; padding-left: 8px; }
/* 数据展示标签 */
QLabel.data {
    color: #2F4F4F;
    font-family: monospace;
    border: 1px solid #E0E0E0;
    background-color: #FFFFFF;
    padding: 6px 12px;
    border-radius: 4px;
}
/* 禁用状态 */
QLabel:disabled { color: #AAAAAA; background-color: transparent; }
/* 链接样式 */
QLabel.link {
    color: #4682B4;
    text-decoration: underline;
    border-bottom: 1px dotted #4682B4;
}
/* 图标标签 */
QLabel.icon {
    qproperty-alignment: AlignCenter;
    border: 1px solid #E0E0E0;
    border-radius: 4px;
    background-color: #FFFFFF;
    padding: 12px;
}
```

#### 字号层级（统一 5 级）

| 级别 | 字号 | 使用场景 |
|------|------|---------|
| 特大 | 30px | 窗口主标题（"纽扣检测"）、步骤标题（"第一步"/"第二步"）、对话框中主要标签 |
| 大 | 23px | 设置标题、报警标签、参数标签、对话框说明文字 |
| 标准 | 18px | 控制面板标签、tab 内标签、通用文本 |
| 小 | 14px | 副标题、版本号、辅助信息 |
| 暗底白字 | 30px/14px | 暗色标题栏专用（color: white） |

> **规范要点**: 精简为 5 级，不再使用 13/15/17/20/25/35px 的散落字号。

---

### 3.6 QTabWidget — 标签页

```css
QTabWidget::pane {
    border: 1px solid #DDD;
    border-radius: 4px;
    background-color: rgb(157, 157, 157);
    top: -1px;
}
QTabWidget::tab-bar { left: 10px; }
QTabBar::tab {
    background-color: #BEBEBE;
    border: 1px solid #DDD;
    border-bottom: none;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    min-width: 80px;
    padding: 5px 10px;
    font: bold 14px;
    color: #666;
}
QTabBar::tab:selected {
    background-color: rgb(157, 157, 157);
    color: #333;
}
QTabBar::tab:hover { background-color: #D0D0D0; }
QTabBar::tab:!selected { margin-top: 2px; }
```

**使用文件**: DlgNewProduction（tab_isNew / tab_type / tab_A / tab_B / tab_C）

---

### 3.7 QProgressBar — 进度条

```css
QProgressBar {
    border: 1px solid #AAAAAA;
    border-radius: 3px;
    background: #FFFFFF;
    color: #444444;
    font-size: 14px;
    text-align: center;
    height: 24px;
}
QProgressBar::chunk {
    background: qlineargradient(
        x1:0, y1:0, x2:1, y2:0,
        stop:0 #888888,
        stop:1 #666666
    );
    border-radius: 2px;
    margin: 2px;
}
/* 状态变体 */
QProgressBar[state="success"]::chunk { /* 绿色渐变 */ }
QProgressBar[state="error"]::chunk   { /* 红色渐变 */ }
QProgressBar[state="warning"]::chunk { /* 黄色渐变 */ }
/* 紧凑模式 */
QProgressBar.compact { height: 16px; font-size: 12px; border-radius: 2px; }
/* 垂直模式 */
QProgressBar.vertical { width: 24px; height: 200px; }
/* 不确定模式（动画条纹） */
QProgressBar:indeterminate::chunk {
    background: repeating-linear-gradient(45deg, #666666, #666666 10px, #888888 10px, #888888 20px);
    animation: progress-stripes 1s linear infinite;
}
```

**使用文件**: DlgLoading

---

### 3.8 QLine — 分隔线

```xml
<widget class="Line" name="line">
    <property name="orientation">
        <enum>Qt::Orientation::Horizontal</enum>  <!-- 或 Vertical -->
    </property>
</widget>
```

**使用场景**: 四工位 2×2 网格的横纵分隔、统计信息区水平分隔

---

### 3.9 QSpacer — 间距控制

大量使用 `QSpacerItem`（horizontalSpacer / verticalSpacer）实现弹性空间分配，配合 layout 的 `stretch` 属性精确控制面板比例。

---

## 4. 导航与交互模式

### 弹窗操作流程

- 统一的 **"退出"** 按钮（`pbtn_exit`）关闭弹窗
- 多步流程使用 **"上一步"**（`pbtn_xxx_preStep`）+ **"下一步"**（`pbtn_xxx_nexStep`）按钮组合
- 无标题栏自定义窗口操作：主窗口使用 `pbtn_minimize` + `pbtn_exit` 图标按钮
- 确认/取消二元选择直接使用大型按钮（如 "是"/"否"）

### 工位管理模式

- 4 工位统一采用 RadioButton 组，命名规范：`rbtn_work1` / `rbtn_work2` / `rbtn_work3` / `rbtn_work4`
- 工位图像区使用 `QGridLayout`（2 行 × 3 列，中间列为垂直分隔线）
- 每个工位对应状态 Label 和数量 Label

### 数据展示模式

- **键值对**: 左侧 `QLabel`（名称）+ 右侧 `QLabel`（值）或 `QPushButton`（可点击修改的值）
- **统计数值着色**:
  - 生产总量 → `rgb(0, 0, 0)` 黑色粗体
  - A/B 斗吹气计数 → `rgb(244, 0, 122)` 品红
  - 剔除速率 → `rgb(89, 89, 89)` 深灰
- **单位显示**: 参数值后紧跟单位标签（`mm` / `个` / `秒`）

### 控件命名规范

| 前缀 | 控件类型 | 示例 |
|------|---------|------|
| `pbtn_` | QPushButton | `pbtn_exit` |
| `rbtn_` | QRadioButton | `rbtn_work1` |
| `cbox_` | QCheckBox | `cbox_isDisplayRec` |
| `label_` | QLabel | `label_produceTotal` |
| `gBox_` | QGroupBox | `gBox_set` |
| `hLayout_` / `vLayout_` / `gLayout_` | QHBoxLayout / QVBoxLayout / QGridLayout | `hLayout_info` |

---

## 5. 全局视觉一致性规范

### 5.1 圆角规范

| 组件 | 圆角值 | 备注 |
|------|--------|------|
| QGroupBox（内容区） | 4px | 浅色底，图像/信息展示 |
| QGroupBox（控制区） | 15px | 中灰底，操作面板 |
| QPushButton | 5px | 统一 5px |
| QRadioButton indicator | 10px | 圆形 |
| QCheckBox indicator | 4px | 小圆角方形 |
| QTabBar tab | 4px | 仅顶部两角 |
| QProgressBar | 3px | — |

### 5.2 间距规范

| 属性 | 值 | 适用组件 |
|------|----|---------|
| `padding` | `6px 14px` | QPushButton（标准） |
| `padding` | `5px 5px` | 标题型 QLabel |
| `padding` | `2px 4px` | 通用 QLabel |
| `padding` | `5px` | QGroupBox 内边距 |
| `spacing` | `8px` | QRadioButton 文字与 indicator 间距 |
| `spacing` | `8-10px` | QCheckBox 文字与 indicator 间距 |
| `QGroupBox::title left` | `10px` | 标题水平偏移 |

### 5.3 字体规范

- 全局使用 `font-weight: bold` 于标题、状态信息、数据值
- 标题下方统一加 `border-bottom: 2px solid #cccccc` 作为视觉分隔
- 数据展示型标签使用 `font-family: monospace` + 白色背景 + 边框
- 暗色背景上的文字统一使用 `color: rgb(255, 255, 255)`
- 全项目字号统一为 5 级：14 / 18 / 23 / 30px，不再使用 13/15/17/20/25/35px 散落值

### 5.4 交互状态覆盖

每个交互控件（QPushButton / QRadioButton / QCheckBox）都覆盖了完整的四态:
- **`:default`** (未选中/未悬停)
- **`:hover`** (鼠标悬停)
- **`:checked`** (选中，仅 Radio/Check)
- **`:pressed`** (按下)
- **`:disabled`** (禁用)

### 5.5 LAYOUT 默认值

```xml
<layoutdefault spacing="6" margin="11"/>
```

所有 `.ui` 文件使用相同的 Qt Designer 默认间距。

---

## 6. 统一设计规范速查 (Unified Design Spec Quick Reference)

### 6.1 配色速查

| 用途 | 统一色值 |
|------|---------|
| 内容区背景 | `#F8F8F8` |
| 控制面板背景 | `rgb(181, 181, 181)` |
| 按钮背景 | `white` / `#FFFFFF` |
| 标题栏暗底 | `rgb(81, 81, 81)` |
| 标题文字（亮底） | `rgb(85, 85, 85)` |
| 正文 | `#444444` / `#333` |
| 次级文字 | `#666666` |
| 关键数据 | `rgb(0, 0, 0)` |
| 吹气计数 | `rgb(244, 0, 122)` |
| 边框常规 | `#DDD` / `#CCC` |
| 分隔线 | `#cccccc` |

### 6.2 控件统一规范

| 控件 | 圆角 | 字号 | 特殊说明 |
|------|------|------|---------|
| QGroupBox（内容区） | 4px | 14px bold | 浅灰底 `#F8F8F8` |
| QGroupBox（控制区） | 15px | 20px bold | 中灰底 `rgb(181,181,181)` |
| QPushButton | 4px | 14/18/23/30px | 白底 + `#CCC` 边框 |
| QRadioButton | 10px (圆) | 18px | 4 色主题切换 indicator 色 |
| QCheckBox | 4px | 18px | 统一蓝色 `#2196F3` 选中态 |
| QTabBar tab | 4px (顶) | 14px bold | 选中同 pane 色 |

### 6.3 组件命名前缀

| 前缀 | 控件 | 示例 |
|------|------|------|
| `pbtn_` | QPushButton | `pbtn_exit` |
| `rbtn_` | QRadioButton | `rbtn_work1` |
| `cbox_` | QCheckBox | `cbox_cameraDisconnect1` |
| `label_` | QLabel | `label_produceTotal` |
| `gBox_` | QGroupBox | `gBox_set` |
| `hLayout_` | QHBoxLayout | `hLayout_info` |
| `vLayout_` | QVBoxLayout | `vLayout_cameraStates` |
| `gLayout_` | QGridLayout | `gLayout_cameraStates` |

> **规范要点**: 统一使用 `cbox_`（全小写），不再使用 `cBox_` 驼峰混合形式。

### 6.4 新建页面检查清单

- [ ] 布局采用「左侧图像 + 右侧控制」范式
- [ ] QGroupBox 内容区用浅色 4px 圆角、控制区用中灰 15px 圆角
- [ ] QPushButton 字号从 14/18/23/30 四级中选择
- [ ] QRadioButton 使用 4 色主题（蓝/黄/橙/灰）之一
- [ ] QCheckBox 统一使用蓝色方形圆角风格
- [ ] QLabel 标题使用 `color: rgb(85,85,85); border-bottom: 2px solid #cccccc`
- [ ] 完整交互四态：default / hover / checked(or pressed) / disabled
- [ ] 所有图标通过 `.qrc` 资源文件引用
- [ ] 控件命名遵循前缀规范（`pbtn_` / `rbtn_` / `cbox_` / `label_` / `gBox_`）

---

## 7. 设计语言核心原则总结

> **工业深灰底 + 白色内容区** — 整体色调沉稳克制，适合工业自动化设备长时间运行下的视觉舒适度。

> **颜色语义化** — 🔵 蓝=功能/模式选择，🟡 黄=设备/硬件配置，🟠 橙=缺陷/质量分拣，🟢 绿=正常/成功，🔴 红=错误/退出。

> **高对比数据展示** — 关键生产数值用大字号、粗体、深色/品红色呈现，确保操作工在产线远距离可读。

> **完整的交互状态覆盖** — 每个交互控件都覆盖了 `:hover` / `:checked` / `:pressed` / `:disabled` 四态。

> **横向分区 + 纵向堆叠** — 页面布局始终遵循 `左侧图像预览 | 右侧操作面板` 的主视觉流，右侧面板自上而下排列控制项。

> **QGroupBox 为主要容器** — 所有逻辑分区均使用 QGroupBox 包裹，形成清晰的视觉层次。

---

## 附录: UI 文件清单

| 文件 | 类型 | 尺寸 | 用途 |
|------|------|------|------|
| `ButtonScanner.ui` | QMainWindow | 2047×1213 | 主界面 |
| `DlgExposureTimeSet.ui` | QDialog | 600×196 | 亮度/曝光时间设置 |
| `DlgLoading.ui` | QDialog | 458×176 | 加载进度 |
| `DlgCreateFeatureBank.ui` | QDialog | 1717×1044 | 创建特征库（好纽扣学习） |
| `DlgNewProduction.ui` | QDialog | 1600×900 | 新产品/新物料向导 |
| `DlgRealTimeImgDis.ui` | QDialog | 1600×900 | 实时图像显示 |
| `DlgSetBasePara.ui` | QDialog | 1130×771 | 设置基本参数 |
| `DlgProduceLineSet.ui` | QDialog | — | 产线设置 |
| `DlgWarningManager.ui` | QDialog | 1698×1063 | 报警管理 |
| `DlgProductSet.ui` | QDialog | — | 产品设置 |
