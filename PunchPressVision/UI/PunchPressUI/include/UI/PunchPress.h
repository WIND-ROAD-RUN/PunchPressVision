#pragma once

#include <QMainWindow>
#include <memory>

#include "halconcpp/HalconCpp.h"

#include "global/GlobalType.hpp"
#include "global/GlobalResult.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class PunchPressClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }
class QButtonGroup;
class QGroupBox;
class QListWidget;
class QPushButton;

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

		// UI 层生命周期：build 在 infrastructure/business/app build 之后调用，
		// 此时配置已加载，可安全读取曝光/增益等参数。
		void build();
		void destroy();

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
		/// <summary>刷新右侧栏已加载模型列表。</summary>
		void refreshLoadedModelsList();

		// 相机参数（曝光/增益）
		void onExposure1Clicked();
		void onGain1Clicked();
		void onExposure2Clicked();
		void onGain2Clicked();

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

		// UI 层配置读取入口：后续可在此扩展光源、PLC 地址等配置的读取
		void loadConfigs();
		void loadCameraConfig();
		void updateCameraParamButtons();

		// 弹出数字键盘编辑整数参数（带范围校验）
		bool inputIntegerParam(QPushButton* button, int& value, int min, int max);

		// 将曝光/增益数值同步到相机与配置并持久化
		void applyExposure(global::CameraIndex idx, int value, int Config::cameraCfg::* member);
		void applyGain(global::CameraIndex idx, int value, int Config::cameraCfg::* member);

		Ui::PunchPressClass* ui;
		app::PunchPressApp& app_;

		// 按钮分组：隔离模式和光源两组 RadioButton 的互斥作用域
		QButtonGroup* modeGroup_{ nullptr };
		QButtonGroup* lightGroup_{ nullptr };

		// 图像显示控件（替换 ui->label_imgDisplay 的 QLabel 占位）
		HalconInteractiveLabel* imageView_{ nullptr };

		// 右侧栏：已加载模型列表
		QGroupBox* loadedModelsGroup_{ nullptr };
		QListWidget* loadedModelsList_{ nullptr };

		// 本地缓存的 UI 所需配置
		Config::cameraCfg cameraCfg_;
	};
}
