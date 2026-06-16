#pragma once

#include <QMainWindow>
#include <memory>

#include "halconcpp/HalconCpp.h"

#include "global/GlobalType.hpp"
#include "global/GlobalResult.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class PunchPressClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

namespace ui
{
	// 主窗口：承载模式切换、图像显示、状态指示。
	// 通过 app::PunchPressApp& 注入，UI 不直接触碰 Business/Infrastructure。
	class PunchPress : public QMainWindow
	{
		Q_OBJECT
	public:
		explicit PunchPress(app::PunchPressApp& app, QWidget* parent = nullptr);
		~PunchPress() override;

	protected:
		void showEvent(QShowEvent* e) override;
		void resizeEvent(QResizeEvent* e) override;

	private slots:
		// 模式切换
		void onDebugToggled(bool checked);
		void onProductionToggled(bool checked);

		// 光源
		void onUpperLightClicked();
		void onLowerLightClicked();

		// 模型管理
		void onModelManager();

		// 系统
		void onExit();

		// 来自 App 层的信号
		void onFrameReady(HalconCpp::HImage image, global::CameraIndex camIdx);
		void onPositionResult(global::PositionResult result);
		void onModeChanged(global::RunMode mode);
		void onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason);
		void onPlcConnectionChanged(bool connected);
		void onStartupCheckFailed(const QString& reason);

	private:
		void buildConnections();
		bool ensureHalconWindow();
		void closeHalconWindow();
		void displayImage(const HalconCpp::HImage& image);

		Ui::PunchPressClass* ui;
		app::PunchPressApp& app_;

		QWidget* halconHost_ = nullptr;
		HalconCpp::HTuple halconWindowHandle_;
		HalconCpp::HImage lastImage_;
	};
}
