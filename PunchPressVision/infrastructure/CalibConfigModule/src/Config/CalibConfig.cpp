#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

#include <filesystem>
#include <fstream>

#include <json/json.h>

namespace Config
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kCalibConfigFile = "calib_config.json";
		constexpr const char* kBackupDir = "backup";

		void replaceFile(const fs::path& tmp, const fs::path& target)
		{
			// NTFS 上 rename 原子替换，不先 remove 避免断电丢失窗口
			std::error_code ec;
			fs::rename(tmp, target, ec);
		}

		// 将文件复制到备份目录（.tmp + rename 保证备份写入也不被断电截断）
		void backupFile(const fs::path& srcFile, const fs::path& backupDir)
		{
			std::error_code ec;
			fs::create_directories(backupDir, ec);
			if (ec) return;
			const fs::path dst = backupDir / srcFile.filename();
			const fs::path tmp = backupDir / (srcFile.filename().string() + ".tmp");
			fs::copy_file(srcFile, tmp, fs::copy_options::overwrite_existing, ec);
			if (!ec)
				replaceFile(tmp, dst);
		}

		// HTuple 元素可能是 double 或 string，逐个序列化到 JSON 数组
		Json::Value tupleToJson(const HalconCpp::HTuple& t)
		{
			Json::Value arr(Json::arrayValue);
			for (Hlong i = 0; i < t.Length(); ++i)
			{
				HalconCpp::HTuple elem = t[i];
				bool isNumeric = true;
				try { elem.D(); }
				catch (const HalconCpp::HException&) { isNumeric = false; }

				if (isNumeric)
					arr.append(elem.D());
				else
					arr.append(static_cast<const char*>(elem.S()));
			}
			return arr;
		}

		HalconCpp::HTuple jsonToTuple(const Json::Value& arr)
		{
			HalconCpp::HTuple t;
			for (Json::ArrayIndex i = 0; i < arr.size(); ++i)
			{
				const auto& elem = arr[i];
				if (elem.isNumeric())
					t.Append(elem.asDouble());
				else if (elem.isString())
					t.Append(elem.asString().c_str());
			}
			return t;
		}

		// 将 CalibConfigItem 序列化到 JSON 对象
		Json::Value itemToJson(const CalibConfigItem& item)
		{
			Json::Value obj;
			obj["cameraParameters"] = tupleToJson(item.cameraParameters);
			obj["cameraPose"] = tupleToJson(item.cameraPose);
			obj["calibrationErrors"] = tupleToJson(item.calibrationErrors);
			obj["cameraExposure"] = item.cameraExposure;
			obj["cameraGain"] = item.cameraGain;
			obj["calibBoardDescrPath"] = item.calibBoardDescrPath;
			obj["calibrationReferenceIndex"] = item.calibrationReferenceIndex;
			return obj;
		}

		// 从 JSON 对象反序列化到 CalibConfigItem
		void jsonToItem(const Json::Value& obj, CalibConfigItem& item)
		{
			if (obj.isMember("cameraParameters"))
				item.cameraParameters = jsonToTuple(obj["cameraParameters"]);
			if (obj.isMember("cameraPose"))
				item.cameraPose = jsonToTuple(obj["cameraPose"]);
			if (obj.isMember("calibrationErrors"))
				item.calibrationErrors = jsonToTuple(obj["calibrationErrors"]);
			if (obj.isMember("cameraExposure"))
				item.cameraExposure = obj["cameraExposure"].asDouble();
			if (obj.isMember("cameraGain"))
				item.cameraGain = obj["cameraGain"].asDouble();
			if (obj.isMember("calibBoardDescrPath"))
				item.calibBoardDescrPath = obj["calibBoardDescrPath"].asString();
			if (obj.isMember("calibrationReferenceIndex"))
				item.calibrationReferenceIndex = obj["calibrationReferenceIndex"].asInt();
		}

		const char* cameraIndexKey(global::CameraIndex idx)
		{
			switch (idx)
			{
			case global::CameraIndex::Camera1: return "Camera1";
			case global::CameraIndex::Camera2: return "Camera2";
			}
			return "Unknown";
		}

		global::CameraIndex keyToCameraIndex(const std::string& key)
		{
			if (key == "Camera2")
				return global::CameraIndex::Camera2;
			return global::CameraIndex::Camera1;
		}
	}

	void CalibConfig::saveInDir(const std::string& dirPath)
	{
		try
		{
			Json::Value root(Json::objectValue);
			for (const auto& [idx, item] : _calibConfigMap)
				root[cameraIndexKey(idx)] = itemToJson(item);

			const fs::path dir(dirPath);
			fs::create_directories(dir);

			const fs::path targetFile = dir / kCalibConfigFile;
			const fs::path tmp = dir / (std::string(kCalibConfigFile) + ".tmp");
			{
				std::ofstream ofs(tmp);
				if (!ofs)
					return;
				Json::StreamWriterBuilder builder;
				builder["indentation"] = "  ";
				std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
				writer->write(root, &ofs);
			}
			replaceFile(tmp, targetFile);
		}
		catch (...)
		{
			// Ignore save errors to avoid crashing the application.
		}
	}

	void CalibConfig::loadInDir(const std::string& dirPath)
	{
		try
		{
			auto tryLoadFrom = [this](const fs::path& file) -> bool
			{
				if (!fs::exists(file))
					return false;

				std::ifstream ifs(file);
				if (!ifs)
					return false;

				Json::CharReaderBuilder builder;
				JSONCPP_STRING errs;
				Json::Value root;
				if (!Json::parseFromStream(builder, ifs, &root, &errs))
					return false;

				if (!root.isObject())
					return false;

				_calibConfigMap.clear();
				for (auto it = root.begin(); it != root.end(); ++it)
				{
					const global::CameraIndex idx = keyToCameraIndex(it.key().asString());
					CalibConfigItem item;
					jsonToItem(*it, item);
					_calibConfigMap[idx] = std::move(item);
				}
				return !_calibConfigMap.empty();
			};

			const fs::path dir(dirPath);
			const fs::path primaryFile = dir / kCalibConfigFile;

			// 先尝试主文件
			if (tryLoadFrom(primaryFile))
			{
				// 开机启动时加载成功 → 备份到 backup/ 供下次断电恢复
				backupFile(primaryFile, dir / kBackupDir);
				return;
			}

			// 主文件损坏或缺失 → 尝试从 backup/ 恢复
			const fs::path backupFilePath = dir / kBackupDir / kCalibConfigFile;
			if (tryLoadFrom(backupFilePath))
			{
				// 恢复成功，立即写回主文件
				saveInDir(dirPath);
			}
		}
		catch (...)
		{
			// Ignore load errors; missing file keeps the default values.
		}
	}
}
