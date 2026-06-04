#include "infrastructure/CalibConfigModule/CalibConfigModule.hpp"

#include "infrastructure/CalibConfigModule/CalibConfigModulePath.hpp"


namespace inf
{
	void CalibConfigModule::build()
	{
		calibConfig.loadInDir(CalibConfigModulePath.RootPath);
	}

	void CalibConfigModule::destroy()
	{
		save();
	}

	void CalibConfigModule::save()
	{
		calibConfig.saveInDir(CalibConfigModulePath.RootPath);
	}
}
