#pragma once

#include <memory>
#include <vector>

#include "global/GlobalInterface.hpp"
#include "Config/VisionCfg.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"
#include "infrastructure/ConfigModule/Config/baseCfg.hpp"
#include "infrastructure/ConfigModule/Config/plcAddressCfg.hpp"
#include "infrastructure/ConfigModule/Config/SetCfg.hpp"

namespace rw::oso
{
	class StorageContext;
}

namespace inf
{
	/// 匹配范围矩形坐标（不依赖 Halcon 类型）
	struct MatchRegionRect
	{
		double row1{ 0.0 };
		double col1{ 0.0 };
		double row2{ 0.0 };
		double col2{ 0.0 };
	};

	class ConfigModule
		: public global::IInfrastructure
	{
	public:
		ConfigModule();
		~ConfigModule();
	private:
		std::unique_ptr<rw::oso::StorageContext> storageContext_;
	public:
		Config::BaseCfg baseCfg;
		Config::cameraCfg cameraCfg;
		Config::PlcAddressCfg plcAddressCfg;
		Config::visionCfg visionCfg;
		Config::SetCfg setCfg;

		// 多识别范围（JSON 文件管理，独立于 OSO）
		std::vector<MatchRegionRect> matchRegions;
		void loadMatchRegions(const std::string& configDir);
		void saveMatchRegions(const std::string& configDir);
		void migrateFromLegacyMatchRegion();  // 旧单矩形 → 新多区域

	public:
		void build() override;
		void destroy() override;
		void save();
	};
}
