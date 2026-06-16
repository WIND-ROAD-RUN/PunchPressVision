// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/ModelEditorDialog.h"
#include "ui_Dlg_createshapemodel.h"

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

// 取消 Win32 MessageBox 宏
#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	ModelEditorDialog::ModelEditorDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::Dlg_createshapemodelClass())
		, app_(app)
	{
		ui->setupUi(this);
		buildConnections();
		view_.ensure(ui->label_imgDisplay);
	}

	ModelEditorDialog::~ModelEditorDialog()
	{
		delete ui;
	}

	void ModelEditorDialog::buildConnections()
	{
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &ModelEditorDialog::onFrameReady, Qt::QueuedConnection);
		// "创建模板"按钮 → 训练并保存模型
		connect(ui->btn_createShapeModel, &QPushButton::clicked, this, [this]() { doCreateModel(); });
		connect(ui->btn_exit, &QPushButton::clicked, this, &QDialog::accept);
	}

	void ModelEditorDialog::onFrameReady(HalconCpp::HImage image)
	{
		lastFrame_ = image;
		view_.ensure(ui->label_imgDisplay);
		view_.display(image);
	}

	bool ModelEditorDialog::doCreateModel()
	{
		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun || !lastFrame_.IsInitialized())
			return false;

		bun::CreateModelRequest req;
		req.trainingImage = lastFrame_;
		req.roi = roi_;
		// 记录当前光源/曝光增益到模型元数据（FR-025）
		if (biz.light_control_bun)
		{
			req.upperLight = biz.light_control_bun->getUpperLightState();
			req.lowerLight = biz.light_control_bun->getLowerLightState();
		}

		Config::ShapeModelInfo outInfo;
		std::string err;
		if (!biz.shape_mode_manager_bun->createModel(req, outInfo, &err))
		{
			rw::rqwu::MessageBox::warning(this, QStringLiteral("创建模型"), QString::fromStdString(err));
			return false;
		}
		rw::rqwu::MessageBox::information(this, QStringLiteral("创建模型"), QStringLiteral("模型已保存"));
		return true;
	}
}
