# HalconWidgets 控件集设计

## 概述

`HalconWidgets` 是 Halcon 图像显示与交互的 Qt 可复用控件集合（CMake 目标 `UI::HalconWidgets`）。按**视图操纵 → 测量标注**的复杂度递进，分为三层。

---

## 三层结构

```
HalconDisplayLabel                  ← L1: 纯显示
├── 职责: Halcon 窗口生命周期 + displayImage() + FillMode(Contain/Cover)
│
└── HalconInteractiveLabel          ← L2: 视图操作 (子类)
    ├── 职责: 滚轮缩放、拖拽平移、复位、适应窗口、1:1 像素、选框放大
    │
    └── ImageViewer 组合它          ← L3: 测量标注 (复合控件)
        职责: 坐标/灰度取色、ROI 框选、双点测距、叠加层管理、截图
```

### 划分依据

| 判断 | 子类 HalconInteractiveLabel | 复合控件 ImageViewer |
|------|:---:|:---:|
| 修改 Halcon SetPart 参数 | ✓ | ✗ |
| 在 Halcon 窗口内操作 | ✓ | — |
| 需要额外 Qt Widget 叠加层 | ✗ | ✓ |
| 需要状态栏 / 信息展示 | ✗ | ✓ |
| 产出数据（坐标、距离）| — | ✓ |

---

## L1: HalconDisplayLabel（已实现）

**路径**: `include/UI/HalconDisplayLabel.h`

自包含的 Halcon 图像显示 QLabel 子类。
- 生命周期: showEvent 懒创建 Halcon 窗口 → resizeEvent 自动同步尺寸 → 析构自动关闭
- 显示模式: `FillMode::Contain`（完整显示+留白）/ `FillMode::Cover`（裁切填满）
- API: `displayImage()`, `clear()`, `setFillMode()`, `lastImage()`, `isReady()`

---

## L2: HalconInteractiveLabel（下一阶段）

**路径**: `include/UI/HalconInteractiveLabel.h`

继承 `HalconDisplayLabel`，增加视图操纵能力。

### 需求清单

| # | 功能 | 描述 |
|---|------|------|
| 1.1 | 滚轮缩放 | 以鼠标指针在图像中的位置为中心缩放，非控件中心 |
| 1.2 | 左键拖拽平移 | 放大后可拖拽浏览图像不同区域，光标变为抓手 |
| 1.3 | 右键复位 | 右键单击恢复 Fit to Window 完整视图 |
| 1.4 | Ctrl+0 适应窗口 | 缩放至图像恰好填满控件（等效 FillMode::Contain） |
| 1.5 | Ctrl+1 实际像素 | 1:1 像素映射，Halcon 窗口 1px = 屏幕 1px |
| 3.5 | 选框局部放大 | Ctrl+左键拖拽绘制选框 → 释放后放大到选框区域 |

### 关键技术点

**鼠标事件捕获**: Halcon 通过 `OpenWindow` 在 QLabel 内创建原生子窗口（HWND），Qt 鼠标事件不会穿透到 Halcon 窗口。解决方案：

| 方案 | 做法 | 选用 |
|------|------|:---:|
| A. Qt 事件过滤器 | `installEventFilter` 不适用（Halcon 窗口非 Qt widget） | ❌ |
| B. Halcon 鼠标回调 | `set_window_param(WindowHandle, "mouse_callback", ...)` 原生回调 → 转发 Qt | ✅ |
| C. 透明 Qt 覆盖层 | 在 Halcon 窗口上方放透明 QWidget 捕获鼠标 | ✅ |

**推荐 B + C 混合**:
- 缩放/平移核心交互用方案 C（透明覆盖层统一处理鼠标 + 可复用为 L3 叠加层画布）
- 方案 B 作为图像坐标查询的辅助通道

### API 示意

```cpp
class HalconInteractiveLabel : public HalconDisplayLabel
{
    Q_OBJECT
public:
    void resetView();           // 复位到完整显示
    void zoomToFit();           // 适应窗口 (Ctrl+0)
    void zoomToActual();        // 1:1 实际像素 (Ctrl+1)
    double currentZoom() const; // 当前缩放比例
    QPointF imagePosAt(const QPoint& widgetPos) const; // 控件坐标 → 图像坐标

signals:
    void zoomChanged(double zoom);
    void viewChanged();

protected:
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    double zoomLevel_{ 1.0 };
    QPointF viewCenter_;        // 图像坐标系下的视口中心
    bool isPanning_{ false };
    QPoint lastMousePos_;
    QPointF panStartCenter_;    // 拖拽起点视口中心
};
```

---

## L3: ImageViewer（后续阶段）

**路径**: `include/UI/ImageViewer.h`

复合控件，组合 `HalconInteractiveLabel` + 半透明叠加层 + 信息标签，提供测量标注能力。

### 结构

```
ImageViewer (QWidget)
├── HalconInteractiveLabel         ← 核心显示区 (占据整个控件)
│   (处理滚轮/拖拽/复位)
├── OverlayWidget (QWidget 透明)   ← 叠加绘制层
│   (ROI 橡皮筋、测距线、十字线、标注)
├── QLabel overlayZoomPercent      ← 右下角半透明缩放倍率
├── QLabel overlayCoordInfo        ← 左下角坐标 + 灰度值
└── ToolBar / ContextMenu          ← 模式切换
```

### 需求清单

| # | 功能 | 描述 |
|---|------|------|
| 2.1 | 像素坐标实时显示 | 状态栏/角落显示 `(row, col)` 和灰度值 |
| 2.2 | ROI 矩形框选 | 左键拖拽画矩形，显示坐标和尺寸 |
| 2.3 | 十字准星光标 | 精确对点模式 |
| 2.4 | 缩放倍率显示 | 角落半透明叠加当前百分比 |
| 3.1 | 悬停取灰度 | 鼠标悬停时在状态栏显示灰度值 |
| 3.2 | 双点测距 | Shift+点击两点 → 像素距离 → 转世界坐标 mm |
| 3.3 | 叠加层开关 | 切换显示/隐藏 ROI/标定标记/检测结果 |
| 3.4 | 截图保存 | Ctrl+S 保存当前视图 |
| 4.x | 未来扩展 | 多图对比、历史帧回放、伪彩色、标注工具 |

### API 示意

```cpp
class ImageViewer : public QWidget
{
    Q_OBJECT
public:
    void displayImage(const HalconCpp::HImage& image);
    void setCrosshairEnabled(bool on);
    void setROIMode(bool on);   // 进入 ROI 框选模式
    HalconCpp::HObject currentROI() const;

    // 叠加层
    void addOverlay(const HalconCpp::HObject& obj, const QString& label);
    void clearOverlays();
    void setOverlayVisible(bool visible);

signals:
    void roiSelected(QRectF imageROI);
    void measurementReady(double pixelDist, double worldDist_mm);
    void mousePosChanged(int row, int col, int grayValue);
};
```

---

## 使用场景对照

| 场景 | 使用控件 | 说明 |
|------|---------|------|
| 模板训练对话框（只看不操作）| `HalconDisplayLabel` | 轻量，无交互开销 |
| 主界面调试模式（缩放查看细节）| `HalconInteractiveLabel` | 需要视图操纵 |
| 标定模式（取点、框 ROI）| `ImageViewer` | 需要 ROI 和坐标 |
| 生产模式（查看检测结果叠加）| `ImageViewer` | 需要叠加层 |

---

## 文件清单

```
UI/Controls/HalconWidgets/
├── DESIGN.md                          ← 本文档
├── CMakeLists.txt                     ← 目标 UI::HalconWidgets
├── include/UI/
│   ├── HalconDisplayLabel.h           ← L1 已实现
│   ├── HalconInteractiveLabel.h       ← L2 待实现
│   ├── ImageViewer.h                  ← L3 待实现
│   └── OverlayWidget.h                ← 叠加绘制层 待实现
└── src/
    ├── HalconDisplayLabel.cpp
    ├── HalconInteractiveLabel.cpp
    ├── ImageViewer.cpp
    └── OverlayWidget.cpp
```
