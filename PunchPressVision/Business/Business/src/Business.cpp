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

		calib_bun = infTool_->calib_bun.get();
		nine_point_bun = infTool_->nine_point_bun.get();
		two_camera_splice_bun = infTool_->two_camera_splice_bun.get();

		camera_bun = std::make_unique<CameraBun>(inf_);
		shape_mode_manager_bun = std::make_unique<ShapeModeManagerBun>(inf_);
		light_control_bun = std::make_unique<LightControlBun>(inf_);

		camera_bun->build();
		shape_mode_manager_bun->build();
		light_control_bun->build();
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
