#include "infrastructure/infrastructure.hpp"

namespace inf
{
	infrastructure::infrastructure()
	{
		config_module_ = std::make_unique<ConfigModule>();
		calib_config_module_ = std::make_unique<CalibConfigModule>();
		nine_point_module_ = std::make_unique<NinePointModule>();
		two_camera_splice_module_ = std::make_unique<TwoCameraSpliceModule>();
		camera_module_ = std::make_unique<CameraModule>();
		shape_model_manager_module_ = std::make_unique<ShapeModelManagerModule>();
	}

	infrastructure::~infrastructure()
	{
		shape_model_manager_module_.reset();
		camera_module_.reset();
		two_camera_splice_module_.reset();
		nine_point_module_.reset();
		calib_config_module_.reset();
		config_module_.reset();
	}

	void infrastructure::build()
	{
		config_module_->build();
		calib_config_module_->build();
		nine_point_module_->build();
		two_camera_splice_module_->build();
		camera_module_->cameraCfg = config_module_->cameraCfg;
		camera_module_->build();
		shape_model_manager_module_->build();
	}

	void infrastructure::destroy()
	{
	}
}
