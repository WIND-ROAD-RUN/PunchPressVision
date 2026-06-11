#pragma once

#include "global/GlobalPath.hpp"

namespace inf
{
	inline struct 
	{
		std::string RootPath = global::joinPath(global::ProjectRootPath, "NinePointModule");
	}NinePointModulePath;
}
