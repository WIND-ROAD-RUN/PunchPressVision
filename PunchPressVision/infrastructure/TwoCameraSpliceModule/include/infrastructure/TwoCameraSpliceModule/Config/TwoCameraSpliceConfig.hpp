#pragma once
#include <string>

namespace inf
{
	namespace Config
	{
		class TwoCameraSpliceCfg
		{
			//TODO:完成双相机拼接的序列化和反序列化以及参数
		public:
			void saveInDir(const std::string& dirPath);
			void loadInDir(const std::string& dirPath);
		};
	}
}
