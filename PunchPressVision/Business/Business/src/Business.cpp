#include "Business/Business.hpp"

namespace bun
{
	Business::Business(inf::infrastructure& inf)
		: inf_(inf)
	{
		// 构造阶段：创建所有子组件对象
		infTool_ = std::make_unique<infTool::infTool>(inf_);

		calib_bun = infTool_->calib_bun.get();
		nine_point_bun = infTool_->nine_point_bun.get();
		two_camera_splice_bun = infTool_->two_camera_splice_bun.get();

		camera_bun = std::make_unique<CameraBun>(inf_, *infTool_);
		shape_mode_manager_bun = std::make_unique<ShapeModeManagerBun>(inf_, *infTool_);
		light_control_bun = std::make_unique<LightControlBun>(inf_, *infTool_);
	}

	Business::~Business()
	{
		// 构造逆序显式 reset，确保依赖关系正确析构
		light_control_bun.reset();
		shape_mode_manager_bun.reset();
		camera_bun.reset();
		infTool_.reset();
	}

	void Business::build()
	{
		// build 阶段：仅调用子组件 build，不再创建对象
		if (infTool_) infTool_->build();
		if (camera_bun) camera_bun->build();
		if (shape_mode_manager_bun) shape_mode_manager_bun->build();
		if (light_control_bun) light_control_bun->build();
	}

	void Business::destroy()
	{
		if (light_control_bun) light_control_bun->destroy();
		if (shape_mode_manager_bun) shape_mode_manager_bun->destroy();
		if (camera_bun) camera_bun->destroy();
		if (infTool_) infTool_->destroy();
	}

	void Business::start()
	{
		if (infTool_) infTool_->start();
		if (camera_bun) camera_bun->start();
		if (shape_mode_manager_bun) shape_mode_manager_bun->start();
		if (light_control_bun) light_control_bun->start();
	}

	void Business::stop()
	{
		if (light_control_bun) light_control_bun->stop();
		if (shape_mode_manager_bun) shape_mode_manager_bun->stop();
		if (camera_bun) camera_bun->stop();
		if (infTool_) infTool_->stop();
	}
}
