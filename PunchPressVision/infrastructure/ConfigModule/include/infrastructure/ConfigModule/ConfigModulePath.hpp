#pragma once

#include "global/GlobalPath.hpp"

namespace inf
{
	inline struct 
	{
	public:
		std::string RootPath = global::ProjectRootPath + "/config";
	public:
		std::string baseCfgName = "baseCfg.xml";
		std::string cameraCfgName = "cameraCfg.xml";
		std::string visionCfgName = "visionCfg.json";
	public:

	}ConfigModulePath;
}