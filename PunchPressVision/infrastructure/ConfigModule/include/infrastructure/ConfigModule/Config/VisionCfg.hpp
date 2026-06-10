#pragma once

#include <fstream>
#include <string>

#include <rwul/core/shapes/core_Rectangle.hpp>

namespace Config
{
	// 视觉算法配置。采用手写键值对 IO 持久化（含几何类型，不走 OSO 轨道）。
	class visionCfg
	{
	public:
		rw::RectanglePixel filterRegion;

	public:
		void save(const std::string& filePath)
		{
			try
			{
				const std::string tmp = filePath + ".tmp";
				std::ofstream ofs(tmp);
				if (!ofs)
					return;
				ofs << "filterLeft=" << filterRegion.leftTop.x << "\n";
				ofs << "filterTop=" << filterRegion.leftTop.y << "\n";
				ofs << "filterRight=" << filterRegion.rightBottom.x << "\n";
				ofs << "filterBottom=" << filterRegion.rightBottom.y << "\n";
				ofs.close();
				// 原子替换
				std::remove(filePath.c_str());
				std::rename(tmp.c_str(), filePath.c_str());
			}
			catch (...)
			{
				// 静默失败
			}
		}

		void load(const std::string& filePath)
		{
			try
			{
				std::ifstream ifs(filePath);
				if (!ifs)
					return;
				std::string line;
				while (std::getline(ifs, line))
				{
					if (!line.empty() && line.back() == '\r')
						line.pop_back();
					const auto pos = line.find('=');
					if (pos == std::string::npos)
						continue;
					const std::string key = line.substr(0, pos);
					const std::string value = line.substr(pos + 1);
					try
					{
						if (key == "filterLeft") filterRegion.leftTop.x = std::stoi(value);
						else if (key == "filterTop") filterRegion.leftTop.y = std::stoi(value);
						else if (key == "filterRight") filterRegion.rightBottom.x = std::stoi(value);
						else if (key == "filterBottom") filterRegion.rightBottom.y = std::stoi(value);
					}
					catch (...)
					{
						continue;
					}
				}
			}
			catch (...)
			{
				// 静默失败：回退默认值
			}
		}
	};
}
