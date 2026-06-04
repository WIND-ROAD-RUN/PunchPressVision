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

		//TODO:读取配置文件，反序列化到baseCfg和cameraCfg,例如路径为RootPath + baseCfgName和RootPath + cameraCfgName
		try
		{
			
		}
		catch (...)
		{
		}
	}

	void ConfigModule::destroy()
	{
		save();
		storageContext_.reset();
	}

	void ConfigModule::save()
	{
		//TODO:保存配置文件，序列化baseCfg和cameraCfg到文件
	}
}
