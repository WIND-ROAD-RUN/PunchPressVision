#include "infrastructure/StampPatternModule/Config/StampPatternItem.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <json/json.h>

#include <QDebug>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Config
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kInfoFile = "pattern_info.json";
		constexpr const char* kParamsFile = "pattern_params.txt";

		// Windows 上 fs::rename 在目标已存在时会失败（非 POSIX 原子替换语义），
		// 失败时退回 MoveFileExW 强制替换，再兜底 remove+rename。
		bool replaceFile(const fs::path& tmp, const fs::path& target)
		{
			std::error_code ec;
			fs::rename(tmp, target, ec);
			if (!ec)
				return true;

#ifdef _WIN32
			if (::MoveFileExW(tmp.c_str(), target.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
				return true;
#endif

			qWarning() << "[StampPatternItem] rename失败:"
				<< QString::fromStdString(tmp.string()) << "->"
				<< QString::fromStdString(target.string())
				<< "错误:" << QString::fromStdString(ec.message());

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
			if (ec)
			{
				qWarning() << "[StampPatternItem] 兜底rename仍失败:"
					<< QString::fromStdString(target.string())
					<< "错误:" << QString::fromStdString(ec.message());
				return false;
			}
			return true;
		}

		bool readJsonSafe(const fs::path& filePath, Json::Value& root)
		{
			if (!fs::exists(filePath))
				return false;
			std::ifstream ifs(filePath);
			if (!ifs)
				return false;

			Json::CharReaderBuilder builder;
			builder["collectComments"] = false;
			std::string errs;
			if (!Json::parseFromStream(builder, ifs, &root, &errs))
				return false;
			return true;
		}

		void writeJsonSafe(const fs::path& filePath, const Json::Value& root)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";

			std::ofstream ofs(tmp);
			if (!ofs)
				return;

			Json::StreamWriterBuilder builder;
			builder["indentation"] = "  ";
			std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
			writer->write(root, &ofs);
			ofs << '\n';
			ofs.close();

			replaceFile(tmp, filePath);
		}

		bool readImageSafe(const fs::path& filePath, HalconCpp::HImage& image)
		{
			if (!fs::exists(filePath))
				return false;
			try
			{
				image.ReadImage(filePath.string().c_str());
			}
			catch (...)
			{
				return false;
			}
			return true;
		}

		void writeParamsSafe(const fs::path& filePath,
			double alignRow, double alignCol, double alignAngle, double alignScale, int alpha,
			bool fromDxf)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";
			std::ofstream ofs(tmp);
			if (!ofs)
				return;
			ofs << "alignRow=" << alignRow << '\n';
			ofs << "alignCol=" << alignCol << '\n';
			ofs << "alignAngle=" << alignAngle << '\n';
			ofs << "alignScale=" << alignScale << '\n';
			ofs << "alpha=" << alpha << '\n';
			ofs << "fromDxf=" << (fromDxf ? 1 : 0) << '\n';
			ofs.close();
			replaceFile(tmp, filePath);
		}

		std::string trimCr(const std::string& s)
		{
			if (!s.empty() && s.back() == '\r')
				return s.substr(0, s.size() - 1);
			return s;
		}

		bool readParamsSafe(const fs::path& filePath,
			double& alignRow, double& alignCol, double& alignAngle, double& alignScale, int& alpha,
			bool& fromDxf)
		{
			if (!fs::exists(filePath))
				return false;
			std::ifstream ifs(filePath);
			if (!ifs)
				return false;
			std::string line;
			while (std::getline(ifs, line))
			{
				line = trimCr(line);
				if (line.empty() || line.front() == '#')
					continue;
				const auto pos = line.find('=');
				if (pos == std::string::npos)
					continue;
				const std::string key = line.substr(0, pos);
				const std::string value = line.substr(pos + 1);
				try
				{
					if (key == "alignRow")
						alignRow = std::stod(value);
					else if (key == "alignCol")
						alignCol = std::stod(value);
					else if (key == "alignAngle")
						alignAngle = std::stod(value);
					else if (key == "alignScale")
						alignScale = std::stod(value);
					else if (key == "alpha")
						alpha = std::stoi(value);
					else if (key == "fromDxf")
						fromDxf = (std::stoi(value) != 0);
				}
				catch (...)
				{
					continue;
				}
			}
			return true;
		}
	}

	bool copyImageFile(const std::string& src, const std::string& dst)
	{
		try
		{
			const fs::path srcPath(src);
			const fs::path dstPath(dst);
			if (!fs::exists(srcPath))
				return false;

			fs::create_directories(dstPath.parent_path());
			const fs::path tmp = dstPath.string() + ".tmp";
			std::error_code ec;
			fs::copy_file(srcPath, tmp, fs::copy_options::overwrite_existing, ec);
			if (ec)
			{
				fs::remove(tmp, ec);
				return false;
			}
			return replaceFile(tmp, dstPath);
		}
		catch (...)
		{
			return false;
		}
	}

	void StampPatternData::loadInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);

			// 加载对齐/显示参数
			readParamsSafe(dirPath / kParamsFile,
				alignRow, alignCol, alignAngle, alignScale, alpha, fromDxf);

			// 加载套版图片
			readImageSafe(dirPath / kPatternImageFileName, _patternImage);
		}
		catch (...)
		{
			// Ignore load errors; missing files keep the default values.
		}
	}

	void StampPatternData::saveInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);

			// 只持久化对齐/显示参数。图片文件由 importStampPattern 以原始字节复制写入，
			// 不经过 Halcon 重新编码，确保透明通道（alpha）不被破坏。
			writeParamsSafe(dirPath / kParamsFile,
				alignRow, alignCol, alignAngle, alignScale, alpha, fromDxf);
		}
		catch (...)
		{
			// Ignore save errors to avoid crashing the application.
		}
	}

	void StampPatternInfo::loadInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);
			folder_path_ = dirPath.string();

			Json::Value root;
			if (!readJsonSafe(dirPath / kInfoFile, root))
				return;

			id_ = root.get("id", id_).asString();
			base_info.name = root.get("name", base_info.name).asString();
			create_time_ = root.get("createTime", create_time_).asString();
			update_time_ = root.get("updateTime", update_time_).asString();
			folder_path_ = root.get("folderPath", folder_path_).asString();
		}
		catch (...)
		{
			// Ignore load errors
		}
	}

	void StampPatternInfo::saveInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);

			Json::Value root;
			root["id"] = id_;
			root["name"] = base_info.name;
			root["createTime"] = create_time_;
			root["updateTime"] = update_time_;
			root["folderPath"] = folder_path_.empty() ? dirPath.string() : folder_path_;

			writeJsonSafe(dirPath / kInfoFile, root);
		}
		catch (...)
		{
			// Ignore save errors
		}
	}

	void StampPatternItem::loadInDir(const std::string& dir)
	{
		info.loadInDir(dir);
		data.loadInDir(dir);
	}

	void StampPatternItem::saveInDir(const std::string& dir)
	{
		info.saveInDir(dir);
		data.saveInDir(dir);
	}
}
