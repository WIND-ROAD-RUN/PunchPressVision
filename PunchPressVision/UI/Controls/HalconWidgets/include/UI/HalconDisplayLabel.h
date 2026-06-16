#pragma once

#include <QLabel>

#include "halconcpp/HalconCpp.h"

namespace ui
{
	/// 自包含的 Halcon 图像显示控件（QLabel 子类）。
	/// 内部管理 Halcon 窗口生命周期（懒创建 + 自动同步尺寸 + 自动析构）。
	/// 支持 Contain（完整显示+留白）和 Cover（填满+裁切）两种显示模式。
	class HalconDisplayLabel : public QLabel
	{
		Q_OBJECT
	public:
		enum class FillMode
		{
			Contain, // 完整显示图像，保持宽高比，可能留白
			Cover,   // 裁切图像填满控件，保持宽高比，不留白
		};

		explicit HalconDisplayLabel(QWidget* parent = nullptr);
		~HalconDisplayLabel() override;

		/// 显示 Halcon 图像（自动同步控件尺寸）
		virtual void displayImage(const HalconCpp::HImage& image);

		/// 清空显示内容
		virtual void clear();

		/// 设置填充模式
		void setFillMode(FillMode mode) { fillMode_ = mode; }

		/// 获取当前填充模式
		FillMode fillMode() const { return fillMode_; }

		/// 获取最后显示的图像
		HalconCpp::HImage lastImage() const { return lastImage_; }

		/// Halcon 窗口句柄（供外部 Halcon 操作使用，如截图等）
		HalconCpp::HTuple halconHandle() const { return handle_; }

		/// Halcon 窗口是否已初始化（showEvent 后为 true）
		bool isReady() const { return windowCreated_; }

	signals:
		/// Halcon 窗口就绪时发射
		void windowReady();

	protected:
		void showEvent(QShowEvent* e) override;
		void resizeEvent(QResizeEvent* e) override;

		void ensureWindow();
		void closeWindow();
		void syncWindowSize();

		HalconCpp::HTuple handle_;
		HalconCpp::HImage lastImage_;
		FillMode fillMode_{ FillMode::Cover };
		bool windowCreated_{ false };
	};
}
