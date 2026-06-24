#pragma once

#include <QWidget>
#include <QPoint>

#include "halconcpp/HalconCpp.h"

namespace ui
{
    /// 轻量 Halcon 显示控件：在给定 QWidget 上打开/复用一个 Halcon 窗口并绘制图像，
    /// 支持鼠标平移、滚轮缩放、双击自适应窗口等交互操作。
    class HalconView
    {
        // -- 内部事件过滤器 ----------------------------------------------------------
        class Filter;

    public:
        ~HalconView();

        /// 在 host 控件上确保有一个 Halcon 窗口（已存在则复用），并创建事件覆盖层。
        bool ensure(QWidget* host);

        /// 在当前窗口显示图像，首帧或图像尺寸变化时自动 fit 视图。
        void display(const HalconCpp::HImage& image);

        /// 缩放至自适应窗口大小。
        void fitToWindow();

        /// 响应宿主控件尺寸变化，调整 Halcon 窗口大小。
        void resizeToHost();

        /// 关闭 Halcon 窗口并清理覆盖层。
        void close();

    private:
        void applyView();

        double fitZoomLevel() const;
        QPointF visibleImageSize() const;

        void clampViewCenter();

        /// 将控件坐标映射为图像坐标。
        QPointF imagePosAt(const QPointF& widgetPos) const;

        // -- 成员变量 ----------------------------------------------------------------
        QWidget* host_ = nullptr;
        HalconCpp::HTuple handle_;
        HalconCpp::HImage last_;

        // 覆盖层与事件过滤
        QWidget* overlay_ = nullptr;
        Filter* filter_ = nullptr;

        // 视图状态
        QPointF viewCenter_;
        double zoomLevel_ = 1.0;
        bool viewInitialized_ = false;

        // 平移状态
        bool panning_ = false;
        QPoint lastMousePos_;
        QPointF panStartCenter_;

        friend class Filter;
    };
}
