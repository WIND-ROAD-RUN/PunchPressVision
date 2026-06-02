#include "infrastructure/ConfigModule/ConfigModule.hpp"

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
	}

	void ConfigModule::destroy()
	{
		storageContext_.reset();
	}
}
