#include "CalibConfigModule.t.hpp"

#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

int main()
{
	inf::Config::CalibConfig config;
	config.loadInDir("path/to/config/dir");
	config.saveInDir("path/to/config/dir");
	return 0;
}