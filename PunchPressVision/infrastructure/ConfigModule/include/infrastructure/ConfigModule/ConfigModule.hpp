#pragma once

#include <memory>

#include "global/GlobalInterface.hpp"
#include "Config/VisionCfg.hpp"
#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"
#include "infrastructure/ConfigModule/Config/baseCfg.hpp"

namespace rw::oso
{
	class StorageContext;
}

namespace inf
{
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
		Config::visionCfg visionCfg;
	public:
		void build() override;
		void destroy() override;
		void save();
	};
}
