#include "infrastructure/NinePointModule/NinePointModule.hpp"

#include "infrastructure/NinePointModule/NinePointModulePath.hpp"

namespace inf
{
	void NinePointModule::build()
	{
		ninePointConfig.loadInDir(NinePointModulePath.RootPath);
	}

	void NinePointModule::destroy()
	{
		if (!skipSaveOnDestroy)
			save();
	}

	void NinePointModule::save()
	{
		ninePointConfig.saveInDir(NinePointModulePath.RootPath);
	}
}
