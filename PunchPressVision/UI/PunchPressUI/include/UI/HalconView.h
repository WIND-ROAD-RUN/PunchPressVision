#pragma once

#include <QWidget>

#include "halconcpp/HalconCpp.h"

namespace ui
{
	// 轻量 Halcon 显示助手：在给定 QWidget 上打开/复用一个 Halcon 窗口并绘制图像。
	// 各对话框/主窗口共享，避免重复的窗口管理代码。
	class HalconView
	{
	public:
		// 在 host 控件上确保有一个 Halcon 窗口（已存在则复用）。
		bool ensure(QWidget* host)
		{
			using namespace HalconCpp;
			host_ = host;
			if (!host_)
				return false;
			if (handle_.Length() > 0)
				return true;
			try
			{
				const Hlong winId = static_cast<Hlong>(host_->winId());
				OpenWindow(0, 0, host_->width(), host_->height(), winId, "visible", "", &handle_);
				SetWindowAttr("background_color", "gray");
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		void display(const HalconCpp::HImage& image)
		{
			using namespace HalconCpp;
			if (!image.IsInitialized() || handle_.Length() == 0)
				return;
			try
			{
				HTuple w, h;
				image.GetImageSize(&w, &h);
				SetPart(handle_, 0, 0, h[0].I() - 1, w[0].I() - 1);
				ClearWindow(handle_);
				DispObj(image, handle_);
				last_ = image;
			}
			catch (...)
			{
			}
		}

		void resizeToHost()
		{
			using namespace HalconCpp;
			if (handle_.Length() == 0 || !host_)
				return;
			try
			{
				SetWindowExtents(handle_, 0, 0, host_->width(), host_->height());
				if (last_.IsInitialized())
					display(last_);
			}
			catch (...)
			{
			}
		}

		void close()
		{
			using namespace HalconCpp;
			if (handle_.Length() == 0)
				return;
			try { CloseWindow(handle_); }
			catch (...) {}
			handle_ = HTuple();
		}

	private:
		QWidget* host_ = nullptr;
		HalconCpp::HTuple handle_;
		HalconCpp::HImage last_;
	};
}
