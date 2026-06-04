#pragma once
#include <string>

namespace inf
{
	namespace Config
	{
		class CalibConfig
		{
			//TODO:完成畸变矫正的序列化和反序列化以及参数
		public:
			void saveInDir(const std::string & dirPath);
			void loadInDir(const std::string & dirPath);
		};
	}
}
