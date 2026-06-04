#pragma once

#include <memory>

#include "infrastructure/ConfigModule/Config/cameraCfg.hpp"
#include "infrastructure/ConfigModule/Config/baseCfg.hpp"

namespace rw::oso
{
	class StorageContext;
}

namespace inf
{
	class ConfigModule
	{
	public:
		ConfigModule();
		~ConfigModule();
	private:
		std::unique_ptr<rw::oso::StorageContext> storageContext_;
	public:
		Config::BaseCfg baseCfg;
		Config::cameraCfg cameraCfg;
	public:
		void build();
		void destroy();
	};
}
