#include "infrastructure/ConfigModule/ConfigModule.hpp"
#include "infrastructure/ConfigModule/ConfigModulePath.hpp"

#include <filesystem>

#include "rwul/oso/oso_StorageContext.hpp"

namespace inf
{
	ConfigModule::ConfigModule()
	{
	}

	ConfigModule::~ConfigModule()
	{
	}

	void ConfigModule::build()
	{
		storageContext_ = std::make_unique<rw::oso::StorageContext>(rw::oso::StorageType::Xml);

		try
		{
			namespace fs = std::filesystem;
			fs::create_directories(ConfigModulePath.RootPath);

			const std::string basePath =
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.baseCfgName);
			const std::string cameraPath =
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.cameraCfgName);

			// 不存在时以默认值写入，保证后续 load 有文件可读
			storageContext_->ensureFileExistsSafe(
				basePath, static_cast<rw::oso::ObjectStoreAssembly>(baseCfg));
			storageContext_->ensureFileExistsSafe(
				cameraPath, static_cast<rw::oso::ObjectStoreAssembly>(cameraCfg));

			// 反序列化；失败时保留默认值（静默降级）
			bool ok = false;
			auto loadedBase = storageContext_->loadSafeToType<Config::BaseCfg>(basePath, ok);
			if (ok)
				baseCfg = loadedBase;

			ok = false;
			auto loadedCamera = storageContext_->loadSafeToType<Config::cameraCfg>(cameraPath, ok);
			if (ok)
				cameraCfg = loadedCamera;

			// visionCfg 采用手写 IO（含 Halcon/几何类型），单独加载
			visionCfg.load(global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.visionCfgName));
		}
		catch (...)
		{
			// 配置损坏时回退到默认值，确保设备能继续运行
		}
	}

	void ConfigModule::destroy()
	{
		save();
		storageContext_.reset();
	}

	void ConfigModule::save()
	{
		if (!storageContext_)
			return;

		try
		{
			namespace fs = std::filesystem;
			fs::create_directories(ConfigModulePath.RootPath);

			storageContext_->saveSafe(
				static_cast<rw::oso::ObjectStoreAssembly>(baseCfg),
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.baseCfgName));
			storageContext_->saveSafe(
				static_cast<rw::oso::ObjectStoreAssembly>(cameraCfg),
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.cameraCfgName));

			visionCfg.save(global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.visionCfgName));
		}
		catch (...)
		{
			// 静默失败
		}
	}
}
