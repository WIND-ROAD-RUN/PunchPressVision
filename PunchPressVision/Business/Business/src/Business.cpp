#include "Business/Business.hpp"

namespace bun
{
	Business::Business(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void Business::build()
	{
		infTool_ = std::make_unique<infTool::infTool>(inf_);
		infTool_->build();

		camera_bun = infTool_->camera_bun.get();
		nine_point_bun = infTool_->nine_point_bun.get();
		two_camera_splice_bun = infTool_->two_camera_splice_bun.get();

		calib_bun = std::make_unique<CalibBun>(inf_);
		shape_mode_manager_bun = std::make_unique<ShapeModeManagerBun>(inf_);
		light_control_bun = std::make_unique<LightControlBun>(inf_);

		calib_bun->build();
		shape_mode_manager_bun->build();
		light_control_bun->build();
	}

	void Business::destroy()
	{
		if (light_control_bun) light_control_bun->destroy();
		if (shape_mode_manager_bun) shape_mode_manager_bun->destroy();
		if (infTool_) infTool_->destroy();
		if (calib_bun) calib_bun->destroy();
	}

	void Business::start()
	{
		if (calib_bun) calib_bun->start();
		if (infTool_) infTool_->start();
		if (shape_mode_manager_bun) shape_mode_manager_bun->start();
		if (light_control_bun) light_control_bun->start();
	}

	void Business::stop()
	{
		if (light_control_bun) light_control_bun->stop();
		if (shape_mode_manager_bun) shape_mode_manager_bun->stop();
		if (infTool_) infTool_->stop();
		if (calib_bun) calib_bun->stop();
	}
}
