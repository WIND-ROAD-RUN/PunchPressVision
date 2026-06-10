#include "Business/Business.hpp"

namespace bun
{
	Business::Business(inf::infrastructure& inf)
		: inf_(inf)
	{

	}

	void Business::build()
	{
		calib_bun = std::make_unique<CalibBun>(inf_);
		two_camera_splice_bun = std::make_unique<TwoCameraSpliceBun>(inf_);
		nine_point_bun = std::make_unique<NinePointBun>(inf_);
		camera_bun = std::make_unique<CameraBun>(inf_);
		shape_mode_manager_bun = std::make_unique<ShapeModeManagerBun>(inf_);
		light_control_bun = std::make_unique<LightControlBun>(inf_);

		calib_bun->build();
		two_camera_splice_bun->build();
		nine_point_bun->build();
		camera_bun->build();
		shape_mode_manager_bun->build();
		light_control_bun->build();
	}

	void Business::destroy()
	{
		light_control_bun->destroy();
		shape_mode_manager_bun->destroy();
		camera_bun->destroy();
		nine_point_bun->destroy();
		two_camera_splice_bun->destroy();
		calib_bun->destroy();
	}

	void Business::start()
	{
		calib_bun->start();
		two_camera_splice_bun->start();
		nine_point_bun->start();
		camera_bun->start();
		shape_mode_manager_bun->start();
		light_control_bun->start();
	}

	void Business::stop()
	{
		light_control_bun->stop();
		shape_mode_manager_bun->stop();
		camera_bun->stop();
		nine_point_bun->stop();
		two_camera_splice_bun->stop();
		calib_bun->stop();
	}
}
