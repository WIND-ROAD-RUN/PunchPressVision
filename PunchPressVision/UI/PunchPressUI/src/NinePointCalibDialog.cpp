#include "UI/NinePointCalibDialog.h"
#include "ui_DlgJiudianbiaoding.h"

#include "app/PunchPressApp.hpp"

namespace ui
{
	NinePointCalibDialog::NinePointCalibDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::DlgJiudianbiaodingClass())
		, app_(app)
	{
		ui->setupUi(this);
		buildConnections();
		view_.ensure(ui->label_imgDisplay);
	}

	NinePointCalibDialog::~NinePointCalibDialog()
	{
		delete ui;
	}

	void NinePointCalibDialog::buildConnections()
	{
		connect(&app_, &app::PunchPressApp::frameReady,
			this, &NinePointCalibDialog::onFrameReady, Qt::QueuedConnection);

		auto& biz = app_.business();
		if (biz.nine_point_bun)
		{
			connect(biz.nine_point_bun, &infTool::NinePointBun::calibrationProgress,
				this, &NinePointCalibDialog::onCalibrationProgress, Qt::QueuedConnection);
			connect(biz.nine_point_bun, &infTool::NinePointBun::calibrationFinished,
				this, &NinePointCalibDialog::onCalibrationFinished, Qt::QueuedConnection);
		}

		// "开始九点标定"按钮 → 启动自动标定流程
		connect(ui->btn_jiudianbiaoding, &QPushButton::clicked,
			this, &NinePointCalibDialog::onStartCalibration);
		connect(ui->btn_exit, &QPushButton::clicked, this, &QDialog::accept);
	}

	void NinePointCalibDialog::onStartCalibration()
	{
		auto& biz = app_.business();
		if (!biz.nine_point_bun)
			return;
		// TODO(硬件/UI): 九点机械坐标应由 UI 输入或配置给出，这里用占位的网格点。
		std::vector<infTool::Point2D> coords;
		for (int i = 0; i < 9; ++i)
			coords.push_back(bun::Point2D{ static_cast<double>((i % 3) * 10),
				static_cast<double>((i / 3) * 10) });
		biz.nine_point_bun->setMechanicalCoords(coords);

		std::string err;
		if (!biz.nine_point_bun->startAutoCalibration(&err))
			setWindowTitle(QStringLiteral("九点标定 - ") + QString::fromStdString(err));
	}

	void NinePointCalibDialog::onFrameReady(HalconCpp::HImage image, global::CameraIndex /*camIdx*/)
	{
		lastFrame_ = image;
		view_.ensure(ui->label_imgDisplay);
		view_.display(image);
	}

	void NinePointCalibDialog::onCalibrationProgress(int currentPoint, int totalPoints)
	{
		setWindowTitle(QStringLiteral("九点标定 %1/%2").arg(currentPoint).arg(totalPoints));
	}

	void NinePointCalibDialog::onCalibrationFinished(bool /*success*/, const QString& message)
	{
		setWindowTitle(QStringLiteral("九点标定 - ") + message);
	}
}
