#include "UI/HalconDisplayLabel.h"

#include <QResizeEvent>
#include <QShowEvent>

namespace ui
{
	HalconDisplayLabel::HalconDisplayLabel(QWidget* parent)
		: QLabel(parent)
	{
	}

	HalconDisplayLabel::~HalconDisplayLabel()
	{
		closeWindow();
	}

	void HalconDisplayLabel::showEvent(QShowEvent* e)
	{
		QLabel::showEvent(e);
		ensureWindow();
		syncWindowSize();
	}

	void HalconDisplayLabel::resizeEvent(QResizeEvent* e)
	{
		QLabel::resizeEvent(e);
		if (windowCreated_)
		{
			syncWindowSize();
			if (lastImage_.IsInitialized())
				displayImage(lastImage_);
		}
	}

	void HalconDisplayLabel::ensureWindow()
	{
		using namespace HalconCpp;
		if (windowCreated_)
			return;

		const int w = width();
		const int h = height();
		if (w <= 0 || h <= 0)
			return;

		try
		{
			const Hlong winId = static_cast<Hlong>(this->winId());
			OpenWindow(0, 0, w, h, winId, "visible", "", &handle_);
			SetWindowAttr("background_color", "gray");
			windowCreated_ = true;
			emit windowReady();
		}
		catch (...)
		{
		}
	}

	void HalconDisplayLabel::closeWindow()
	{
		using namespace HalconCpp;
		if (!windowCreated_)
			return;
		try { CloseWindow(handle_); }
		catch (...) {}
		handle_ = HTuple();
		windowCreated_ = false;
	}

	void HalconDisplayLabel::syncWindowSize()
	{
		using namespace HalconCpp;
		if (!windowCreated_)
			return;
		try
		{
			SetWindowExtents(handle_, 0, 0, width(), height());
		}
		catch (...) {}
	}

	void HalconDisplayLabel::displayImage(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;
		if (!image.IsInitialized())
			return;

		ensureWindow();
		if (!windowCreated_)
			return;

		try
		{
			syncWindowSize();

			HTuple iw, ih;
			image.GetImageSize(&iw, &ih);
			const double imgW = static_cast<double>(iw[0].I());
			const double imgH = static_cast<double>(ih[0].I());
			const int lw = width();
			const int lh = height();

			if (lw <= 0 || lh <= 0)
			{
				SetPart(handle_, 0, 0, imgH - 1, imgW - 1);
			}
			else if (fillMode_ == FillMode::Cover)
			{
				// 填满模式：裁切图像以匹配控件宽高比
				const double imgAspect = imgW / imgH;
				const double labelAspect = static_cast<double>(lw) / lh;

				int row1, col1, row2, col2;
				if (imgAspect > labelAspect)
				{
					// 图像更宽 → 高度填满，左右裁切
					const double dispH = imgH;
					const double dispW = imgH * labelAspect;
					const int colOff = static_cast<int>((imgW - dispW) / 2.0);
					row1 = 0;
					col1 = colOff;
					row2 = static_cast<int>(dispH) - 1;
					col2 = colOff + static_cast<int>(dispH * labelAspect) - 1;
				}
				else
				{
					// 图像更高/等 → 宽度填满，上下裁切
					const double dispW = imgW;
					const double dispH = imgW / labelAspect;
					const int rowOff = static_cast<int>((imgH - dispH) / 2.0);
					row1 = rowOff;
					col1 = 0;
					row2 = rowOff + static_cast<int>(dispW / labelAspect) - 1;
					col2 = static_cast<int>(dispW) - 1;
				}
				SetPart(handle_, row1, col1, row2, col2);
			}
			else
			{
				// 完整显示模式
				SetPart(handle_, 0, 0, imgH - 1, imgW - 1);
			}

			ClearWindow(handle_);
			DispObj(image, handle_);
			lastImage_ = image;
		}
		catch (...)
		{
		}
	}

	void HalconDisplayLabel::clear()
	{
		using namespace HalconCpp;
		if (!windowCreated_)
			return;
		try
		{
			ClearWindow(handle_);
		}
		catch (...) {}
	}
}
