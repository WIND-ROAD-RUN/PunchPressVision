#pragma once

#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModule.hpp"
#include "infrastructure/CalibConfigModule/CalibConfigModule.hpp"
#include "infrastructure/CameraModule/CameraModule.hpp"
#include "infrastructure/ConfigModule/ConfigModule.hpp"
#include "infrastructure/NinePointModule/NinePointModule.hpp"
#include "infrastructure/TwoCameraSpliceModule/TwoCameraSpliceModule.hpp"
#include "infrastructure/ControlModule/ControlModule.hpp"
#include "infrastructure/LightIOModule/LightIOModule.hpp"

namespace inf
{
	class infrastructure
	{
	public:
		infrastructure();
		~infrastructure();
	public:
		void build();
		void destroy();
	public:
		std::unique_ptr<ConfigModule> config_module_;
		std::unique_ptr<CalibConfigModule> calib_config_module_;
		std::unique_ptr<TwoCameraSpliceModule> two_camera_splice_module_;
		std::unique_ptr<NinePointModule> nine_point_module_;
		std::unique_ptr<CameraModule> camera_module_;
		std::unique_ptr<ShapeModelManagerModule> shape_model_manager_module_;
		std::unique_ptr<ControlModule> control_module_;
		std::unique_ptr<LightIOModule> light_io_module_;
	};
}
