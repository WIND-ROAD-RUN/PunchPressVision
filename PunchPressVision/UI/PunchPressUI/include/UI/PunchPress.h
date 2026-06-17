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
class QButtonGroup;

namespace ui
{
	class HalconInteractiveLabel;

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
		// 模式切换（clicked 用于支持再次点击取消选中）
		void onDebugClicked();
		void onWorkClicked();

		// 光源
		void onUpperLightClicked();
		void onLowerLightClicked();

		// 模型管理
		void onModelManager();

		// 系统
		void onExit();

		// 来自 App 层的信号
		void onFrameReady(HalconCpp::HImage image);
		void onPositionResult(global::PositionResult result);
		void onModeChanged(global::RunMode mode);
		void onCameraConnectionChanged(global::CameraIndex idx, bool connected, QString reason);
		void onPlcConnectionChanged(bool connected);
		void onStartupCheckFailed(const QString& reason);

	private:
		void buildConnections();
		void setupImageView();

		Ui::PunchPressClass* ui;
		app::PunchPressApp& app_;

		// 按钮分组：隔离模式和光源两组 RadioButton 的互斥作用域
		QButtonGroup* modeGroup_{ nullptr };
		QButtonGroup* lightGroup_{ nullptr };

		// 图像显示控件（替换 ui->label_imgDisplay 的 QLabel 占位）
		HalconInteractiveLabel* imageView_{ nullptr };
	};
}
