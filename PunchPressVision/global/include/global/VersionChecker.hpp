#pragma once

#include <string>
#include <optional>

namespace global {

/// <summary>返回编译到二进制中的构建 ID（时间戳_git短哈希）。</summary>
std::string getEmbeddedBuildId();

/// <summary>读取可执行文件目录下的 build.version 文件内容。</summary>
std::string getWorkDirBuildId();

/// <summary>
/// 检查嵌入式构建 ID 与工作目录 build.version 是否一致。
/// 工作目录无 build.version 时（首次运行）自动写入并返回 nullopt；
/// 一致时返回 nullopt；不一致时返回错误描述字符串。
/// </summary>
std::optional<std::string> checkVersionConsistency();

/// <summary>将嵌入式构建 ID 覆写到工作目录 build.version 文件。</summary>
void updateWorkDirBuildId();

}
