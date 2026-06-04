#include "infrastructure/TwoCameraSpliceModule/TwoCameraSpliceModule.hpp"

#include "infrastructure/TwoCameraSpliceModule/TwoCameraSpliceModulePath.hpp"

namespace inf
{
	void TwoCameraSpliceModule::build()
	{
		twoCameraSpliceConfig.loadInDir(TwoCameraSpliceModulePath.RootPath);
	}

	void TwoCameraSpliceModule::destroy()
	{
		save();
	}

	void TwoCameraSpliceModule::save()
	{
		twoCameraSpliceConfig.saveInDir(TwoCameraSpliceModulePath.RootPath);
	}
}
