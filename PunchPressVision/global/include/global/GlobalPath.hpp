#pragma once
#include <string>

namespace global
{
	// 项目运行时数据根目录（配置/标定/模型/日志均位于其下）
	inline std::string ProjectRootPath = "D:/zfkjData/PunchPressVision/";

	// 拼接子目录，保证使用统一的 '/' 分隔符
	inline std::string joinPath(const std::string& base, const std::string& sub)
	{
		if (base.empty())
			return sub;
		if (base.back() == '/' || base.back() == '\\')
			return base + sub;
		return base + "/" + sub;
	}

	namespace path
	{
		inline std::string configDir() { return joinPath(ProjectRootPath, "config"); }
		inline std::string modelDir() { return joinPath(ProjectRootPath, "ShapeModels"); }
		inline std::string calibDir() { return joinPath(ProjectRootPath, "calib"); }
		inline std::string logDir() { return joinPath(ProjectRootPath, "logs"); }
	}
}
