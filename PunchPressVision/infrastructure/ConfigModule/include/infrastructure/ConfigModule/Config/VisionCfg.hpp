#pragma once

#include <string>

#include <rwul/core/shapes/core_Rectangle.hpp>

namespace Config
{
	class visionCfg
	{
		//TODO:完成序列化和反序列接口
	public:
		rw::RectanglePixel filterRegion;
	public:
		//TODO:保存到配置文件中
		void save(const std::string & filePath)
		{
			
		}

		void load(const std::string & filePath)
		{
			
		}
	};
}
