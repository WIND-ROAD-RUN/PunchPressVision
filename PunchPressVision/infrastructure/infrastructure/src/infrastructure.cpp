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
		control_module_ = std::make_unique<ControlModule>();
		light_io_module_ = std::make_unique<LightIOModule>();
	}

	infrastructure::~infrastructure()
	{
		// 构造逆序显式 reset，确保依赖关系正确析构
		light_io_module_.reset();
		control_module_.reset();
		shape_model_manager_module_.reset();
		camera_module_.reset();
		two_camera_splice_module_.reset();
		nine_point_module_.reset();
		calib_config_module_.reset();
		config_module_.reset();
	}

	void infrastructure::build()
	{
		// 1. 配置最先构建（其他模块依赖配置）
		config_module_->build();

		// 2. 标定/九点/拼接配置
		calib_config_module_->build();
		nine_point_module_->build();
		two_camera_splice_module_->build();

		// 3. 相机（依赖配置中的相机参数与 IP）
		camera_module_->cameraCfg = config_module_->cameraCfg;
		camera_module_->baseCfg = config_module_->baseCfg;
		camera_module_->build();

		// 4. 模型管理器
		shape_model_manager_module_->build();

		// 5. PLC 控制网关
		control_module_->build();

		// 6. 光源 IO（依赖 PLC 控制网关）
		light_io_module_->setControlModule(control_module_.get());
		light_io_module_->build();
	}

	void infrastructure::destroy()
	{
		// 反向顺序销毁：后构建的先销毁
		if (light_io_module_) light_io_module_->destroy();
		if (control_module_) control_module_->destroy();
		if (shape_model_manager_module_) shape_model_manager_module_->destroy();
		if (camera_module_) camera_module_->destroy();
		if (two_camera_splice_module_) two_camera_splice_module_->destroy();
		if (nine_point_module_) nine_point_module_->destroy();
		if (calib_config_module_) calib_config_module_->destroy();
		if (config_module_) config_module_->destroy();
	}
}
