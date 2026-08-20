#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

#include <filesystem>
#include <fstream>

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

		constexpr const char* kCalibConfigFile = "calib_config.json";
		constexpr const char* kBackupDir = "backup";

		void replaceFile(const fs::path& tmp, const fs::path& target)
		{
			// NTFS 上 rename 原子替换，不先 remove 避免断电丢失窗口。
			// 注意：Windows 上 fs::rename 在目标已存在时会失败（非 POSIX 语义），
			// 失败时退回 MoveFileExW 强制替换，再兜底 remove+rename，
			// 避免 .tmp 残留、新参数永远不生效。
			std::error_code ec;
			fs::rename(tmp, target, ec);
			if (!ec)
				return;

#ifdef _WIN32
			if (::MoveFileExW(tmp.c_str(), target.c_str(),
				MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0)
				return;
#endif

			qWarning() << "[CalibConfig] rename失败:"
				<< QString::fromStdString(tmp.string()) << "->"
				<< QString::fromStdString(target.string())
				<< "错误:" << QString::fromStdString(ec.message());

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
			if (ec)
			{
				qWarning() << "[CalibConfig] 兜底rename仍失败:"
					<< QString::fromStdString(target.string())
					<< "错误:" << QString::fromStdString(ec.message());
			}
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
		qDebug() << "[CalibConfig] 保存标定参数, 目录:" << QString::fromStdString(dirPath)
			<< "相机数=" << _calibConfigMap.size();
		try
		{
			Json::Value root(Json::objectValue);
			for (const auto& [idx, item] : _calibConfigMap)
			{
				root[cameraIndexKey(idx)] = itemToJson(item);
				qDebug() << "[CalibConfig] 保存" << cameraIndexKey(idx)
					<< ": paramsLen=" << item.cameraParameters.Length()
					<< "poseLen=" << item.cameraPose.Length()
					<< "board=" << QString::fromStdString(item.calibBoardDescrPath)
					<< "exposure=" << item.cameraExposure
					<< "gain=" << item.cameraGain;
			}

			const fs::path dir(dirPath);
			fs::create_directories(dir);

			const fs::path targetFile = dir / kCalibConfigFile;
			const fs::path tmp = dir / (std::string(kCalibConfigFile) + ".tmp");
			{
				std::ofstream ofs(tmp);
				if (!ofs)
				{
					qWarning() << "[CalibConfig] 无法写入临时文件:" << QString::fromStdString(tmp.string());
					return;
				}
				Json::StreamWriterBuilder builder;
				builder["indentation"] = "  ";
				std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
				writer->write(root, &ofs);
			}
			replaceFile(tmp, targetFile);
			qDebug() << "[CalibConfig] 保存完成:" << QString::fromStdString(targetFile.string());
		}
		catch (const std::exception& e)
		{
			qWarning() << "[CalibConfig] 保存异常:" << e.what();
		}
		catch (...)
		{
			qWarning() << "[CalibConfig] 保存发生未知异常";
		}
	}

	void CalibConfig::loadInDir(const std::string& dirPath)
	{
		qDebug() << "[CalibConfig] 加载标定参数, 目录:" << QString::fromStdString(dirPath);
		try
		{
			auto tryLoadFrom = [this](const fs::path& file) -> bool
			{
				if (!fs::exists(file))
				{
					qDebug() << "[CalibConfig] 文件不存在:" << QString::fromStdString(file.string());
					return false;
				}

				std::ifstream ifs(file);
				if (!ifs)
				{
					qWarning() << "[CalibConfig] 文件无法打开:" << QString::fromStdString(file.string());
					return false;
				}

				Json::CharReaderBuilder builder;
				JSONCPP_STRING errs;
				Json::Value root;
				if (!Json::parseFromStream(builder, ifs, &root, &errs))
				{
					qWarning() << "[CalibConfig] JSON解析失败:" << QString::fromStdString(file.string())
						<< "错误:" << QString::fromStdString(errs);
					return false;
				}

				if (!root.isObject())
				{
					qWarning() << "[CalibConfig] JSON根节点不是对象:" << QString::fromStdString(file.string());
					return false;
				}

				_calibConfigMap.clear();
				for (auto it = root.begin(); it != root.end(); ++it)
				{
					const global::CameraIndex idx = keyToCameraIndex(it.key().asString());
					CalibConfigItem item;
					jsonToItem(*it, item);
					qDebug() << "[CalibConfig] 加载" << it.key().asString().c_str()
						<< ": paramsLen=" << item.cameraParameters.Length()
						<< "poseLen=" << item.cameraPose.Length()
						<< "errorsLen=" << item.calibrationErrors.Length()
						<< "board=" << QString::fromStdString(item.calibBoardDescrPath)
						<< "exposure=" << item.cameraExposure
						<< "gain=" << item.cameraGain
						<< "refIndex=" << item.calibrationReferenceIndex;
					_calibConfigMap[idx] = std::move(item);
				}
				qDebug() << "[CalibConfig] 从" << QString::fromStdString(file.string())
					<< "加载完成, 相机数=" << _calibConfigMap.size();
				return !_calibConfigMap.empty();
			};

			const fs::path dir(dirPath);
			const fs::path primaryFile = dir / kCalibConfigFile;

			// 先尝试主文件
			if (tryLoadFrom(primaryFile))
			{
				qDebug() << "[CalibConfig] 主文件加载成功，备份到backup/";
				// 开机启动时加载成功 → 备份到 backup/ 供下次断电恢复
				backupFile(primaryFile, dir / kBackupDir);
				return;
			}

			qWarning() << "[CalibConfig] 主文件加载失败，尝试从backup/恢复";
			// 主文件损坏或缺失 → 尝试从 backup/ 恢复
			const fs::path backupFilePath = dir / kBackupDir / kCalibConfigFile;
			if (tryLoadFrom(backupFilePath))
			{
				qDebug() << "[CalibConfig] 从backup恢复成功，写回主文件";
				// 恢复成功，立即写回主文件
				saveInDir(dirPath);
			}
			else
			{
				qWarning() << "[CalibConfig] 无可用标定参数：主文件与backup均缺失或损坏";
			}
		}
		catch (const std::exception& e)
		{
			qWarning() << "[CalibConfig] 加载异常:" << e.what();
		}
		catch (...)
		{
			qWarning() << "[CalibConfig] 加载发生未知异常";
		}
	}
}
