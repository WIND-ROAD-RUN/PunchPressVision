#include "infrastructure/ConfigModule/ConfigModule.hpp"
#include "infrastructure/ConfigModule/ConfigModulePath.hpp"

#include <filesystem>
#include <fstream>

#include <json/json.h>

#include "rwul/oso/oso_StorageContext.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace inf
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kMatchRegionsFile = "match_regions.json";
		constexpr const char* kBackupDir = "backup";

		// Windows 上 fs::rename 在目标已存在时会失败（非 POSIX 原子替换语义），
		// 失败时退回 MoveFileExW 强制替换，再兜底 remove+rename，
		// 避免 .tmp 残留、正式文件永远不更新。
		void replaceFile(const fs::path& tmp, const fs::path& target)
		{
			std::error_code ec;
			fs::rename(tmp, target, ec);
			if (!ec)
				return;

#ifdef _WIN32
			if (::MoveFileExW(tmp.c_str(), target.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
				return;
#endif

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
		}

		void writeJsonSafe(const fs::path& filePath, const Json::Value& root)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";
			{
				std::ofstream ofs(tmp);
				if (!ofs)
					return;
				Json::StreamWriterBuilder builder;
				builder["indentation"] = "  ";
				std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
				writer->write(root, &ofs);
			}
			replaceFile(tmp, filePath);
		}

		bool readJsonSafe(const fs::path& filePath, Json::Value& root)
		{
			if (!fs::exists(filePath))
				return false;
			std::ifstream ifs(filePath);
			if (!ifs)
				return false;
			Json::CharReaderBuilder builder;
			JSONCPP_STRING errs;
			return Json::parseFromStream(builder, ifs, &root, &errs);
		}
	}

	ConfigModule::ConfigModule()
	{
	}

	ConfigModule::~ConfigModule()
	{
	}

	void ConfigModule::loadMatchRegions(const std::string& configDir)
	{
		matchRegions.clear();
		try
		{
			const fs::path filePath = fs::path(configDir) / kMatchRegionsFile;

			// 旧版本 rename 静默失败会留下含最新数据的 .tmp，能解析则收养
			const fs::path tmp = filePath.string() + ".tmp";
			std::error_code ec;
			if (fs::exists(tmp, ec))
			{
				Json::Value tmpRoot;
				if (readJsonSafe(tmp, tmpRoot))
					replaceFile(tmp, filePath);
				else
					fs::remove(tmp, ec);
			}

			Json::Value root;
			if (!readJsonSafe(filePath, root) || !root.isArray())
				return;

			for (const auto& item : root)
			{
				MatchRegionRect r;
				r.row1 = item.get("row1", 0.0).asDouble();
				r.col1 = item.get("col1", 0.0).asDouble();
				r.row2 = item.get("row2", 0.0).asDouble();
				r.col2 = item.get("col2", 0.0).asDouble();
				matchRegions.push_back(r);
			}
		}
		catch (...) {}
	}

	void ConfigModule::saveMatchRegions(const std::string& configDir)
	{
		try
		{
			Json::Value root(Json::arrayValue);
			for (const auto& r : matchRegions)
			{
				Json::Value item;
				item["row1"] = r.row1;
				item["col1"] = r.col1;
				item["row2"] = r.row2;
				item["col2"] = r.col2;
				root.append(item);
			}

			const fs::path dir(configDir);
			const fs::path filePath = dir / kMatchRegionsFile;
			writeJsonSafe(filePath, root);

			// 备份
			const fs::path backupDir = dir / kBackupDir;
			std::error_code ec;
			fs::create_directories(backupDir, ec);
			if (!ec && fs::exists(filePath))
				fs::copy_file(filePath, backupDir / kMatchRegionsFile,
					fs::copy_options::overwrite_existing, ec);
		}
		catch (...) {}
	}

	void ConfigModule::migrateFromLegacyMatchRegion()
	{
		if (!setCfg.matchRegionValid || !matchRegions.empty())
			return;

		MatchRegionRect r;
		r.row1 = setCfg.matchRegionRow1;
		r.col1 = setCfg.matchRegionCol1;
		r.row2 = setCfg.matchRegionRow2;
		r.col2 = setCfg.matchRegionCol2;
		matchRegions.push_back(r);

		setCfg.matchRegionValid = false;
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
			const std::string plcPath =
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.plcAddressCfgName);
			const std::string setCfgPath =
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.setCfgName);

			// 不存在时以默认值写入，保证后续 load 有文件可读
			storageContext_->ensureFileExistsSafe(
				basePath, static_cast<rw::oso::ObjectStoreAssembly>(baseCfg));
			storageContext_->ensureFileExistsSafe(
				cameraPath, static_cast<rw::oso::ObjectStoreAssembly>(cameraCfg));
			storageContext_->ensureFileExistsSafe(
				plcPath, static_cast<rw::oso::ObjectStoreAssembly>(plcAddressCfg));
			storageContext_->ensureFileExistsSafe(
				setCfgPath, static_cast<rw::oso::ObjectStoreAssembly>(setCfg));

			// 反序列化；失败时保留默认值（静默降级）
			bool ok = false;
			auto loadedBase = storageContext_->loadSafeToType<Config::BaseCfg>(basePath, ok);
			if (ok)
				baseCfg = loadedBase;

			ok = false;
			auto loadedCamera = storageContext_->loadSafeToType<Config::cameraCfg>(cameraPath, ok);
			if (ok)
				cameraCfg = loadedCamera;

			ok = false;
			auto loadedPlc = storageContext_->loadSafeToType<Config::PlcAddressCfg>(plcPath, ok);
			if (ok)
				plcAddressCfg = loadedPlc;

			ok = false;
			auto loadedSet = storageContext_->loadSafeToType<Config::SetCfg>(setCfgPath, ok);
			if (ok)
				setCfg = loadedSet;

			// visionCfg 采用手写 IO（含 Halcon/几何类型），单独加载
			visionCfg.load(global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.visionCfgName));

			// 加载多识别范围
			loadMatchRegions(ConfigModulePath.RootPath);

			// 向后兼容：旧单矩形迁移
			migrateFromLegacyMatchRegion();
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
			storageContext_->saveSafe(
				static_cast<rw::oso::ObjectStoreAssembly>(plcAddressCfg),
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.plcAddressCfgName));
			storageContext_->saveSafe(
				static_cast<rw::oso::ObjectStoreAssembly>(setCfg),
				global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.setCfgName));

			visionCfg.save(global::joinPath(ConfigModulePath.RootPath, ConfigModulePath.visionCfgName));

			// 保存多识别范围
			saveMatchRegions(ConfigModulePath.RootPath);
		}
		catch (...)
		{
			// 静默失败
		}
	}
}
