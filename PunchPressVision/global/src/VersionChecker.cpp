#include "global/VersionChecker.hpp"
#include "global/BuildVersion.hpp"

#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace global {

namespace {

std::filesystem::path getExeDirectory()
{
	wchar_t buffer[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, buffer, MAX_PATH);
	return std::filesystem::path(buffer).parent_path();
}

std::string trim(const std::string& str)
{
	const size_t first = str.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return {};
	const size_t last = str.find_last_not_of(" \t\r\n");
	return str.substr(first, last - first + 1);
}

}

std::string getEmbeddedBuildId()
{
	return PPV_BUILD_ID;
}

std::string getWorkDirBuildId()
{
	const auto versionFile = getExeDirectory() / "build.version";
	if (!std::filesystem::exists(versionFile))
	{
		return {};
	}

	std::ifstream ifs(versionFile);
	if (!ifs)
	{
		return {};
	}

	std::string id((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	return trim(id);
}

std::optional<std::string> checkVersionConsistency()
{
	const std::string embedded = getEmbeddedBuildId();
	const std::string workDir = getWorkDirBuildId();

	if (workDir.empty())
	{
		// 首次运行，创建工作目录版本文件
		const auto versionFile = getExeDirectory() / "build.version";
		std::ofstream ofs(versionFile);
		if (ofs)
		{
			ofs << embedded;
		}
		return std::nullopt;
	}

	if (embedded != workDir)
	{
		return std::string("检测到程序版本不一致！\n当前程序构建ID: ") + embedded
			+ "\n工作目录版本ID: " + workDir
			+ "\n\n请确保所有程序文件来自同一次构建发布，并统一更新。"
			+ "\n请勿只替换部分EXE文件，否则可能导致配置丢失或运行异常。";
	}

	return std::nullopt;
}

void updateWorkDirBuildId()
{
	const auto versionFile = getExeDirectory() / "build.version";
	std::ofstream ofs(versionFile);
	if (ofs)
	{
		ofs << getEmbeddedBuildId();
	}
}

}
