#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

#include <filesystem>
#include <fstream>
#include <json/json.h>

namespace Config
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kModelInfoFile = "model_info.json";
		constexpr const char* kParamsFile = "model_params.txt";
		constexpr const char* kTemplateImageFile = "template_image.jpg";
		constexpr const char* kOriginalImageFile = "original_image.jpg";
		constexpr const char* kAnnotatedImageFile = "annotated.jpg";
		constexpr const char* kModelFile = "model.shm";
		constexpr const char* kMetrologyFile = "metrology.mmc";
		constexpr const char* kPaintCreateRoiPrefix = "paint_create_roi_";
		constexpr const char* kPaintShieldRoiPrefix = "paint_shield_roi_";
		constexpr const char* kRoiExtension = ".hobj";
		constexpr const char* kFindCreateXldFile = "find_create_xld.hobj";
		constexpr const char* kFindCreateXldSecondaryFile = "find_create_xld_secondary.hobj";
		constexpr const char* kJpegFormat = "jpeg";
		constexpr int kJpegQuality = 90;

		void replaceFile(const fs::path& tmp, const fs::path& target)
		{
			if (fs::exists(target))
			{
				try { fs::remove(target); }
				catch (...) {}
			}
			fs::rename(tmp, target);
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

		void writeImageSafe(const HalconCpp::HObject& image, const fs::path& filePath)
		{
			if (!image.IsInitialized())
				return;
			try
			{
				// FullDomain 展开 reduced-domain 图，防止 WriteImage 在 ROI 裁剪图上失败
				HalconCpp::HImage fullImage;
				HalconCpp::FullDomain(HalconCpp::HImage(image), &fullImage);
				fs::create_directories(filePath.parent_path());
				fs::path tmp = filePath;
				fullImage.WriteImage(kJpegFormat, 0, tmp.string().c_str());
			}
			catch (...) {}
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

		void writeObjectSafe(const HalconCpp::HObject& obj, const fs::path& filePath)
		{
			if (!obj.IsInitialized())
				return;
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			try { HalconCpp::WriteObject(obj, tmp.string().c_str()); }
			catch (...) { return; }
		}

		bool readObjectSafe(const fs::path& filePath, HalconCpp::HObject& obj)
		{
			if (!fs::exists(filePath))
				return false;
			try
			{
				HalconCpp::ReadObject(&obj, filePath.string().c_str());
			}
			catch (...)
			{
				return false;
			}
			return true;
		}

		void clearOldRoiFiles(const fs::path& dirPath, const std::string& prefix)
		{
			size_t index = 0;
			while (true)
			{
				fs::path filePath = dirPath / (prefix + std::to_string(index) + kRoiExtension);
				if (!fs::exists(filePath))
					break;
				try { fs::remove(filePath); }
				catch (...) {}
				++index;
			}
		}

		void writeTupleSafe(const fs::path& filePath, const HalconCpp::HTuple& tuple)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";
			HalconCpp::WriteTuple(tuple, tmp.string().c_str());
			replaceFile(tmp, filePath);
		}

		bool readTupleSafe(const fs::path& filePath, HalconCpp::HTuple& tuple)
		{
			if (!fs::exists(filePath))
				return false;
			try
			{
				HalconCpp::ReadTuple(filePath.string().c_str(), &tuple);
			}
			catch (...)
			{
				return false;
			}
			return true;
		}

		void writeParamsSafe(const fs::path& filePath,
			int createModelPreProcessType,
			double centerX, double centerY,
			double findCenterX, double findCenterY,
			double offsetX, double offsetY, double offsetAngle,
			int findNumber,
			int singleChannelType,
			double createModelExposureTime, double createModelGain,
			bool upperLight, bool lowerLight,
			bool createModelUseOpening, int createModelOpeningRadius,
			bool createModelUseClosing, int createModelClosingRadius,
			bool createModelUseMean, int createModelMeanRadius,
			int numLevels, double angleStart, double angleExtent, double angleStep,
			const std::string& optimization, const std::string& metric,
			int contrast, int minContrast,
			const std::string& modelPath)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";
			std::ofstream ofs(tmp);
			if (!ofs)
				return;
			ofs << "createModelPreProcessType=" << createModelPreProcessType << '\n';
			ofs << "centerX=" << centerX << '\n';
			ofs << "centerY=" << centerY << '\n';
			ofs << "findCenterX=" << findCenterX << '\n';
			ofs << "findCenterY=" << findCenterY << '\n';
			ofs << "offsetX=" << offsetX << '\n';
			ofs << "offsetY=" << offsetY << '\n';
			ofs << "offsetAngle=" << offsetAngle << '\n';
			ofs << "findnumber=" << findNumber << '\n';
			ofs << "singleChannelType=" << singleChannelType << '\n';
			ofs << "createModelExposureTime=" << createModelExposureTime << '\n';
			ofs << "createModelGain=" << createModelGain << '\n';
			ofs << "upperLight=" << (upperLight ? 1 : 0) << '\n';
			ofs << "lowerLight=" << (lowerLight ? 1 : 0) << '\n';
			ofs << "createModelUseOpening=" << (createModelUseOpening ? 1 : 0) << '\n';
			ofs << "createModelOpeningRadius=" << createModelOpeningRadius << '\n';
			ofs << "createModelUseClosing=" << (createModelUseClosing ? 1 : 0) << '\n';
			ofs << "createModelClosingRadius=" << createModelClosingRadius << '\n';
			ofs << "createModelUseMean=" << (createModelUseMean ? 1 : 0) << '\n';
			ofs << "createModelMeanRadius=" << createModelMeanRadius << '\n';
			ofs << "numLevels=" << numLevels << '\n';
			ofs << "angleStart=" << angleStart << '\n';
			ofs << "angleExtent=" << angleExtent << '\n';
			ofs << "angleStep=" << angleStep << '\n';
			ofs << "optimization=" << optimization << '\n';
			ofs << "metric=" << metric << '\n';
			ofs << "contrast=" << contrast << '\n';
			ofs << "minContrast=" << minContrast << '\n';
			ofs << "modelPath=" << modelPath << '\n';
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
			int& createModelPreProcessType,
			double& centerX, double& centerY,
			double& findCenterX, double& findCenterY,
			double& offsetX, double& offsetY, double& offsetAngle,
			int& findNumber,
			int& singleChannelType,
			double& createModelExposureTime, double& createModelGain,
			bool& upperLight, bool& lowerLight,
			bool& createModelUseOpening, int& createModelOpeningRadius,
			bool& createModelUseClosing, int& createModelClosingRadius,
			bool& createModelUseMean, int& createModelMeanRadius,
			int& numLevels, double& angleStart, double& angleExtent, double& angleStep,
			std::string& optimization, std::string& metric,
			int& contrast, int& minContrast,
			std::string& modelPath)
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
					if (key == "createModelPreProcessType")
						createModelPreProcessType = std::stoi(value);
					else if (key == "centerX")
						centerX = std::stod(value);
					else if (key == "centerY")
						centerY = std::stod(value);
					else if (key == "findCenterX")
						findCenterX = std::stod(value);
					else if (key == "findCenterY")
						findCenterY = std::stod(value);
					else if (key == "offsetX")
						offsetX = std::stod(value);
					else if (key == "offsetY")
						offsetY = std::stod(value);
					else if (key == "offsetAngle")
						offsetAngle = std::stod(value);
					else if (key == "findnumber")
						findNumber = std::stoi(value);
					else if (key == "singleChannelType")
						singleChannelType = std::stoi(value);
					else if (key == "createModelExposureTime")
						createModelExposureTime = std::stod(value);
					else if (key == "createModelGain")
						createModelGain = std::stod(value);
					else if (key == "upperLight")
						upperLight = std::stoi(value) != 0;
					else if (key == "lowerLight")
						lowerLight = std::stoi(value) != 0;
					else if (key == "createModelUseOpening")
						createModelUseOpening = std::stoi(value) != 0;
					else if (key == "createModelOpeningRadius")
						createModelOpeningRadius = std::stoi(value);
					else if (key == "createModelUseClosing")
						createModelUseClosing = std::stoi(value) != 0;
					else if (key == "createModelClosingRadius")
						createModelClosingRadius = std::stoi(value);
					else if (key == "createModelUseMean")
						createModelUseMean = std::stoi(value) != 0;
					else if (key == "createModelMeanRadius")
						createModelMeanRadius = std::stoi(value);
					else if (key == "numLevels")
						numLevels = std::stoi(value);
					else if (key == "angleStart")
						angleStart = std::stod(value);
					else if (key == "angleExtent")
						angleExtent = std::stod(value);
					else if (key == "angleStep")
						angleStep = std::stod(value);
					else if (key == "optimization")
						optimization = value;
					else if (key == "metric")
						metric = value;
					else if (key == "contrast")
						contrast = std::stoi(value);
					else if (key == "minContrast")
						minContrast = std::stoi(value);
					else if (key == "modelPath")
						modelPath = value;
				}
				catch (...)
				{
					continue;
				}
			}
			return true;
		}
	}

	void ShapeModelData::loadInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);

			// 加载基本参数
			readParamsSafe(dirPath / kParamsFile,
				_createModelPreProcessType,
				centerX, centerY,
				findCenterX, findCenterY,
				offsetX, offsetY, offsetAngle,
				findnumber,
				_SingleChannelType,
				_createModelExposureTime, _createModelGain,
				upperLight, lowerLight,
				_createModelUseOpening, _createModelOpeningRadius,
				_createModelUseClosing, _createModelClosingRadius,
				_createModelUseMean, _createModelMeanRadius,
				numLevels, angleStart, angleExtent, angleStep,
				optimization, metric,
				contrast, minContrast,
				modelPath);

			// 加载图像
			readImageSafe(dirPath / kTemplateImageFile, _templateMatImage);
			readImageSafe(dirPath / kOriginalImageFile, _originalImage);
			readImageSafe(dirPath / kAnnotatedImageFile, _annotatedImage);

			// 加载 ShapeModel
			if (fs::exists(dirPath / kModelFile))
			{
				try { HalconCpp::ReadShapeModel((dirPath / kModelFile).string().c_str(), &hv_ModelID); }
				catch (...) {}
			}

			// 加载 MetrologyModel
			if (fs::exists(dirPath / kMetrologyFile))
			{
				try { HalconCpp::ReadMetrologyModel((dirPath / kMetrologyFile).string().c_str(), &hv_MetrologyHandle); }
				catch (...) {}
			}

			// 加载绘制 ROI 列表（支持回撤的历史记录）
			_paintCreateRoiList.clear();
			for (size_t i = 0; ; ++i)
			{
				fs::path filePath = dirPath / (std::string(kPaintCreateRoiPrefix) + std::to_string(i) + kRoiExtension);
				if (!fs::exists(filePath))
					break;
				HalconCpp::HObject obj;
				if (readObjectSafe(filePath, obj))
					_paintCreateRoiList.push_back(obj);
			}

			_paintShieldRoiList.clear();
			for (size_t i = 0; ; ++i)
			{
				fs::path filePath = dirPath / (std::string(kPaintShieldRoiPrefix) + std::to_string(i) + kRoiExtension);
				if (!fs::exists(filePath))
					break;
				HalconCpp::HObject obj;
				if (readObjectSafe(filePath, obj))
					_paintShieldRoiList.push_back(obj);
			}

			// 加载 XLD
			readObjectSafe(dirPath / kFindCreateXldFile, _findCreateXldObj);
			readObjectSafe(dirPath / kFindCreateXldSecondaryFile, _findCreateXldObj_Secondary);
		}
		catch (...)
		{
			// Ignore load errors; missing files keep the default values.
		}
	}

	void ShapeModelData::saveInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);

			// 保存基本参数
			writeParamsSafe(dirPath / kParamsFile,
				_createModelPreProcessType,
				centerX, centerY,
				findCenterX, findCenterY,
				offsetX, offsetY, offsetAngle,
				findnumber,
				_SingleChannelType,
				_createModelExposureTime, _createModelGain,
				upperLight, lowerLight,
				_createModelUseOpening, _createModelOpeningRadius,
				_createModelUseClosing, _createModelClosingRadius,
				_createModelUseMean, _createModelMeanRadius,
				numLevels, angleStart, angleExtent, angleStep,
				optimization, metric,
				contrast, minContrast,
				modelPath);

			// 保存图像（逐个 try-catch 防止一个失败导致后续全部跳过）
			if (_templateMatImage.IsInitialized())
			{
				try { writeImageSafe(_templateMatImage, dirPath / kTemplateImageFile); }
				catch (...) {}
			}
			if (_originalImage.IsInitialized())
			{
				try { writeImageSafe(_originalImage, dirPath / kOriginalImageFile); }
				catch (...) {}
			}
			if (_annotatedImage.IsInitialized())
			{
				try { writeImageSafe(_annotatedImage, dirPath / kAnnotatedImageFile); }
				catch (...) {}
			}

			// 保存 ShapeModel
			if (hv_ModelID.TupleLength() > 0)
			{
				try { HalconCpp::WriteShapeModel(hv_ModelID, (dirPath / kModelFile).string().c_str()); }
				catch (...) {}
			}

			// 保存 MetrologyModel
			if (hv_MetrologyHandle.TupleLength() > 0)
			{
				try { HalconCpp::WriteMetrologyModel(hv_MetrologyHandle, (dirPath / kMetrologyFile).string().c_str()); }
				catch (...) {}
			}

			// 保存绘制 ROI 列表（支持回撤）
			clearOldRoiFiles(dirPath, kPaintCreateRoiPrefix);
			for (size_t i = 0; i < _paintCreateRoiList.size(); ++i)
			{
				if (_paintCreateRoiList[i].IsInitialized())
				{
					auto fileName = std::string(kPaintCreateRoiPrefix) + std::to_string(i) + kRoiExtension;
					writeObjectSafe(_paintCreateRoiList[i], dirPath / fileName);
				}
			}

			clearOldRoiFiles(dirPath, kPaintShieldRoiPrefix);
			for (size_t i = 0; i < _paintShieldRoiList.size(); ++i)
			{
				if (_paintShieldRoiList[i].IsInitialized())
				{
					auto fileName = std::string(kPaintShieldRoiPrefix) + std::to_string(i) + kRoiExtension;
					writeObjectSafe(_paintShieldRoiList[i], dirPath / fileName);
				}
			}

			// 保存 XLD
			if (_findCreateXldObj.IsInitialized())
				writeObjectSafe(_findCreateXldObj, dirPath / kFindCreateXldFile);
			if (_findCreateXldObj_Secondary.IsInitialized())
				writeObjectSafe(_findCreateXldObj_Secondary, dirPath / kFindCreateXldSecondaryFile);
		}
		catch (...)
		{
			// Ignore save errors to avoid crashing the application.
		}
	}

	void ShapeModelInfo::loadInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);
			folder_path_ = dirPath.string();

			Json::Value root;
			if (!readJsonSafe(dirPath / kModelInfoFile, root))
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

	void ShapeModelInfo::saveInDir(const std::string& dir)
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

			writeJsonSafe(dirPath / kModelInfoFile, root);
		}
		catch (...)
		{
			// Ignore save errors
		}
	}

	void ShapeModelItem::loadInDir(const std::string& dir)
	{
		info.loadInDir(dir);
		data.loadInDir(dir);
	}

	void ShapeModelItem::saveInDir(const std::string& dir)
	{
		info.saveInDir(dir);
		data.saveInDir(dir);
	}
}
