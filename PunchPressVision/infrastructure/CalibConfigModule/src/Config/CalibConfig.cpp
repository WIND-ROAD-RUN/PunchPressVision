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

		void replaceFile(const fs::path& tmp, const fs::path& target)
		{
			std::error_code ec;
			if (fs::exists(target, ec))
				fs::remove(target, ec);
			fs::rename(tmp, target, ec);
		}

		// HTuple 元素可能是 double 或 string，逐个序列化到 JSON 数组
		Json::Value tupleToJson(const HalconCpp::HTuple& t)
		{
			Json::Value arr(Json::arrayValue);
			for (Hlong i = 0; i < t.Length(); ++i)
			{
				HalconCpp::HTuple elem = t[i];
				// 尝试以 double 读取，失败则当作 string
				bool isNumeric = true;
				try { elem.D(); }
				catch (const HalconCpp::HException&) { isNumeric = false; }

				if (isNumeric)
					arr.append(elem.D());
				else
					arr.append(elem.S());
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
	}

	void CalibConfig::saveInDir(const std::string& dirPath)
	{
		try
		{
			Json::Value root;
			root["cameraParameters"] = tupleToJson(cameraParameters);
			root["cameraPose"] = tupleToJson(cameraPose);
			root["calibrationErrors"] = tupleToJson(calibrationErrors);
			root["cameraExposure"] = cameraExposure;
			root["cameraGain"] = cameraGain;
			root["calibrationReferenceIndex"] = calibrationReferenceIndex;

			const fs::path dir(dirPath);
			fs::create_directories(dir);

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
			replaceFile(tmp, dir / kCalibConfigFile);
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
			cameraExposure = 10000.0;
			cameraGain = 5.0;
			calibrationReferenceIndex = 0;

			const fs::path filePath = fs::path(dirPath) / kCalibConfigFile;
			if (!fs::exists(filePath))
				return;

			std::ifstream ifs(filePath);
			if (!ifs)
				return;

			Json::CharReaderBuilder builder;
			JSONCPP_STRING errs;
			Json::Value root;
			if (!Json::parseFromStream(builder, ifs, &root, &errs))
				return;

			if (root.isMember("cameraParameters"))
				cameraParameters = jsonToTuple(root["cameraParameters"]);
			if (root.isMember("cameraPose"))
				cameraPose = jsonToTuple(root["cameraPose"]);
			if (root.isMember("calibrationErrors"))
				calibrationErrors = jsonToTuple(root["calibrationErrors"]);
			if (root.isMember("cameraExposure"))
				cameraExposure = root["cameraExposure"].asDouble();
			if (root.isMember("cameraGain"))
				cameraGain = root["cameraGain"].asDouble();
			if (root.isMember("calibrationReferenceIndex"))
				calibrationReferenceIndex = root["calibrationReferenceIndex"].asInt();
		}
		catch (...)
		{
			// Ignore load errors; missing file keeps the default values.
		}
	}
}
