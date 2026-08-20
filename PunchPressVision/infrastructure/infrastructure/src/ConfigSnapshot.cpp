#include "infrastructure/ConfigSnapshot.hpp"

#include "global/GlobalPath.hpp"
#include "infrastructure/ConfigModule/ConfigModulePath.hpp"

#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

#include <QDebug>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace inf
{
	namespace
	{
		namespace fs = std::filesystem;

		// Windows 上 fs::rename 在目标已存在时会失败（非 POSIX 原子替换语义），
		// 失败时退回 MoveFileExW 强制替换，再兜底 remove+rename。
		void replaceFile(const fs::path& tmp, const fs::path& target)
		{
			std::error_code ec;
			fs::rename(tmp, target, ec);
			if (!ec)
				return;

#ifdef _WIN32
			if (::MoveFileExW(tmp.c_str(), target.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
				return;
#endif

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
		}

		// 弱 JSON 完整性检查：被截断/写坏的 JSON 首尾括号必然不配对。
		// 不引入完整解析，仅用于识别"加载必定失败"的文件。
		bool jsonLooksIntact(const fs::path& p)
		{
			std::ifstream ifs(p, std::ios::binary);
			if (!ifs)
				return false;
			const std::string content((std::istreambuf_iterator<char>(ifs)),
				std::istreambuf_iterator<char>());
			const auto first = content.find_first_not_of(" \t\r\n");
			const auto last = content.find_last_not_of(" \t\r\n");
			if (first == std::string::npos)
				return false;
			return (content[first] == '{' && content[last] == '}')
				|| (content[first] == '[' && content[last] == ']');
		}

		// 文件"可用"判定：存在、非空；JSON 额外要求首尾括号配对（识别截断）。
		bool fileUsable(const fs::path& p)
		{
			std::error_code ec;
			if (!fs::is_regular_file(p, ec) || ec)
				return false;
			const auto size = fs::file_size(p, ec);
			if (ec || size == 0)
				return false;
			if (p.extension() == ".json")
				return jsonLooksIntact(p);
			return true;
		}

		// 增量拷贝：目标已是最新（大小与修改时间一致）则跳过；
		// 经 tmp+replace 写入，保证快照文件任何时候都是完整的。
		bool copyIncremental(const fs::path& src, const fs::path& dst)
		{
			std::error_code ec;
			if (fs::is_regular_file(dst, ec) && !ec)
			{
				std::error_code ec1, ec2;
				const bool sameSize = fs::file_size(src, ec1) == fs::file_size(dst, ec2) && !ec1 && !ec2;
				const bool sameTime = fs::last_write_time(src, ec1) == fs::last_write_time(dst, ec2) && !ec1 && !ec2;
				if (sameSize && sameTime)
					return false;
			}

			fs::create_directories(dst.parent_path(), ec);
			ec.clear();
			const fs::path tmp = dst.string() + ".sstmp";
			fs::copy_file(src, tmp, fs::copy_options::overwrite_existing, ec);
			if (ec)
				return false;
			replaceFile(tmp, dst);
			// 对齐时间戳，保证下次增量比较稳定
			fs::last_write_time(dst, fs::last_write_time(src, ec), ec);
			return true;
		}

		// 项目根目录（去掉尾部分隔符）。ProjectRootPath 自带结尾 '/'，
		// 不去掉的话 path 末尾会多出一个空元素，lexically_relative 匹配失败返回空路径。
		fs::path projectRootDir()
		{
			std::string root = global::ProjectRootPath;
			while (!root.empty() && (root.back() == '/' || root.back() == '\\'))
				root.pop_back();
			return fs::path(root);
		}

		// 关键配置文件清单：{主文件, 快照文件}。
		// 子目录名字面量与各模块 *Path.hpp 中的定义保持一致（config / CalibConfigModule /
		// NinePointModule / TwoCameraSpliceModule / ShapeModels）。
		// 注意：不直接引用那些模块 Path 全局变量——它们是「无名 struct 的 inline 变量」，
		// 实际每个翻译单元各持一份拷贝（MSVC 对无名类 inline 变量的处理），
		// 这里必须基于 global::ProjectRootPath 现场推导，保证路径唯一可信来源。
		// match_regions.json / last_loaded_models.json 为 ConfigModule / ShapeModeManagerBun
		// 内部的固定文件名（见 kMatchRegionsFile / saveLastLoadedModels）。
		const std::vector<std::pair<fs::path, fs::path>>& criticalFilePairs()
		{
			static const std::vector<std::pair<fs::path, fs::path>> pairs = []
			{
				const fs::path snapshotRoot(global::path::configSnapshotDir());
				const fs::path root = projectRootDir();

				std::vector<std::pair<fs::path, fs::path>> v;
				auto add = [&](const std::string& subDir, const std::string& name)
				{
					v.emplace_back(root / subDir / name, snapshotRoot / subDir / name);
				};

				add("config", ConfigModulePath.baseCfgName);
				add("config", ConfigModulePath.cameraCfgName);
				add("config", ConfigModulePath.plcAddressCfgName);
				add("config", ConfigModulePath.setCfgName);
				add("config", ConfigModulePath.visionCfgName);
				add("config", "match_regions.json");
				add("config", "last_loaded_models.json");

				add("CalibConfigModule", "calib_config.json");

				add("NinePointModule", "hom_mat_2d.tup");
				add("NinePointModule", "nine_point_params.txt");

				add("TwoCameraSpliceModule", "camera1_picture.bmp");
				add("TwoCameraSpliceModule", "camera2_picture.bmp");
				add("TwoCameraSpliceModule", "map_single1.hobj");
				add("TwoCameraSpliceModule", "map_single2.hobj");
				add("TwoCameraSpliceModule", "two_camera_splice_params.txt");

				return v;
			}();
			return pairs;
		}

		// 主文件不可用且快照可用时，从快照恢复主文件。返回是否发生了恢复。
		bool restoreFile(const fs::path& mainFile, const fs::path& snapshotFile)
		{
			if (fileUsable(mainFile))
				return false;
			if (!fileUsable(snapshotFile))
				return false;

			std::error_code ec;
			fs::create_directories(mainFile.parent_path(), ec);
			ec.clear();
			const fs::path tmp = mainFile.string() + ".rstmp";
			fs::copy_file(snapshotFile, tmp, fs::copy_options::overwrite_existing, ec);
			if (ec)
				return false;
			replaceFile(tmp, mainFile);
			fs::last_write_time(mainFile, fs::last_write_time(snapshotFile, ec), ec);

			qDebug() << "[ConfigSnapshot] 主配置丢失/损坏，已从快照恢复:"
				<< QString::fromStdString(mainFile.string());
			return true;
		}

		bool isTmpResidue(const fs::path& p)
		{
			return p.filename().string().find(".tmp") != std::string::npos
				|| p.extension() == ".sstmp"
				|| p.extension() == ".rstmp";
		}

		// ShapeModels 整树恢复：仅在主目录整体丢失（不存在或为空）时触发，
		// 单个模型文件夹缺失不恢复——那可能是用户有意删除的。
		void restoreModelTreeIfNeeded(const fs::path& mainDir, const fs::path& snapshotDir)
		{
			std::error_code ec;
			bool mainGone = !fs::is_directory(mainDir, ec) || ec;
			if (!mainGone)
				mainGone = fs::is_empty(mainDir, ec) && !ec;
			if (!mainGone)
				return;
			if (!fs::is_directory(snapshotDir, ec) || ec)
				return;

			int restored = 0;
			for (fs::recursive_directory_iterator it(snapshotDir, fs::directory_options::skip_permission_denied, ec), end;
				it != end && !ec; it.increment(ec))
			{
				if (!it->is_regular_file(ec) || isTmpResidue(it->path()))
					continue;
				std::error_code relEc;
				const fs::path rel = it->path().lexically_relative(snapshotDir);
				if (copyIncremental(it->path(), mainDir / rel))
					++restored;
			}
			if (restored > 0)
				qDebug() << "[ConfigSnapshot] ShapeModels 目录丢失，已从快照整树恢复，文件数:" << restored;
		}

		// 将目录树增量同步到快照：跳过 .tmp 残留与模块内部 backup/ 目录。
		int snapshotTree(const fs::path& mainDir, const fs::path& snapshotDir)
		{
			std::error_code ec;
			if (!fs::is_directory(mainDir, ec) || ec)
				return 0;

			int copied = 0;
			for (fs::recursive_directory_iterator it(mainDir, fs::directory_options::skip_permission_denied, ec), end;
				it != end && !ec; it.increment(ec))
			{
				if (!it->is_regular_file(ec) || isTmpResidue(it->path()))
					continue;

				const fs::path rel = it->path().lexically_relative(mainDir);
				bool inBackupDir = false;
				for (const auto& part : rel)
					if (part == "backup")
					{
						inBackupDir = true;
						break;
					}
				if (inBackupDir)
					continue;

				if (copyIncremental(it->path(), snapshotDir / rel))
					++copied;
			}
			return copied;
		}
	}

	void restoreConfigSnapshotIfNeeded()
	{
		try
		{
			const fs::path snapshotRoot(global::path::configSnapshotDir());
			std::error_code ec;
			if (!fs::is_directory(snapshotRoot, ec) || ec)
				return;  // 首次运行尚无快照

			int restored = 0;
			for (const auto& [main, snapshot] : criticalFilePairs())
				if (restoreFile(main, snapshot))
					++restored;

			restoreModelTreeIfNeeded(
				projectRootDir() / "ShapeModels",
				snapshotRoot / "ShapeModels");

			if (restored > 0)
				qDebug() << "[ConfigSnapshot] 启动恢复完成，恢复关键配置文件数:" << restored;
		}
		catch (...)
		{
			// 恢复失败不阻断启动，各模块自身仍有默认值/内部 backup 兜底
		}
	}

	void updateConfigSnapshot()
	{
		try
		{
			int copied = 0;
			for (const auto& [main, snapshot] : criticalFilePairs())
				if (fileUsable(main) && copyIncremental(main, snapshot))
					++copied;

			copied += snapshotTree(
				projectRootDir() / "ShapeModels",
				fs::path(global::path::configSnapshotDir()) / "ShapeModels");

			if (copied > 0)
				qDebug() << "[ConfigSnapshot] 配置快照已更新，同步文件数:" << copied;
		}
		catch (...)
		{
			// 快照失败不影响主流程
		}
	}
}
