#include "infrastructure/infrastructure.hpp"

namespace inf
{
	void infrastructure::build()
	{
		config_module_->build();
		calib_config_module_->build();
		nine_point_module_->build();
		two_camera_splice_module_->build();
		camera_module_->build(config_module_->cameraCfg);
		shape_model_manager_module_->build();
	}

	void infrastructure::destroy()
	{
	}
}
