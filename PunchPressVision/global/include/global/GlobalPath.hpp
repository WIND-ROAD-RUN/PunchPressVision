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

		// 配置快照目录：放在数据根目录的"同级"（如 D:/zfkjData/PunchPressVision_ConfigBackup），
		// 这样整个数据根目录被误删/损坏时快照依然存活，可用于启动时恢复。
		inline std::string configSnapshotDir()
		{
			std::string root = ProjectRootPath;
			while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
				root.pop_back();
			const std::string::size_type pos = root.find_last_of("/\\");
			const std::string leaf = (pos == std::string::npos) ? root : root.substr(pos + 1);
			const std::string parent = (pos == std::string::npos) ? std::string() : root.substr(0, pos);
			return joinPath(parent, leaf + "_ConfigBackup");
		}
	}
}
