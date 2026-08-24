#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
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

			qWarning() << "[ShapeModelItem] rename失败:"
				<< QString::fromStdString(tmp.string()) << "->"
				<< QString::fromStdString(target.string())
				<< "错误:" << QString::fromStdString(ec.message());

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
			if (ec)
			{
				qWarning() << "[ShapeModelItem] 兜底rename仍失败:"
					<< QString::fromStdString(target.string())
					<< "错误:" << QString::fromStdString(ec.message());
				return false;
			}
			return true;
		}

		// 实测 Halcon 24.11：WriteImage/WriteObject 在文件名不以该格式可识别的
		// 扩展名结尾时会自动追加（x.jpg.tmp 会被写成 x.jpg.tmp.jpg）。
		// 给 Halcon 的临时文件名必须把 .tmp 插在扩展名之前（x.tmp.jpg），
		// 保证实际写出的文件名可预测。
		fs::path halconTmpPath(const fs::path& target)
		{
			return target.parent_path() /
				(target.stem().string() + ".tmp" + target.extension().string());
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
				// 临时名必须把 .tmp 插在扩展名之前，否则 Halcon 会再追加一个
				// .jpg（x.jpg.tmp → x.jpg.tmp.jpg），rename 找不到源文件
				const fs::path tmp = halconTmpPath(filePath);
				fullImage.WriteImage(kJpegFormat, 0, tmp.string().c_str());
				replaceFile(tmp, filePath);
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
			// 同上：x.hobj.tmp 会被 WriteObject 写成 x.hobj.tmp.hobj
			const fs::path tmp = halconTmpPath(filePath);
			try
			{
				HalconCpp::WriteObject(obj, tmp.string().c_str());
				replaceFile(tmp, filePath);
			}
			catch (...) {}
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
			double createModelExposureTime2, double createModelGain2,
			bool upperLight, bool lowerLight,
			bool createModelUseOpening, int createModelOpeningRadius,
			bool createModelUseClosing, int createModelClosingRadius,
			bool createModelUseMean, int createModelMeanRadius,
			double angleStart, double angleExtent,
			int contrast, int minContrast, double minScore,
			const std::string& modelPath,
			const std::string& stampPatternId)
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
			ofs << "createModelExposureTime2=" << createModelExposureTime2 << '\n';
			ofs << "createModelGain2=" << createModelGain2 << '\n';
			ofs << "upperLight=" << (upperLight ? 1 : 0) << '\n';
			ofs << "lowerLight=" << (lowerLight ? 1 : 0) << '\n';
			ofs << "createModelUseOpening=" << (createModelUseOpening ? 1 : 0) << '\n';
			ofs << "createModelOpeningRadius=" << createModelOpeningRadius << '\n';
			ofs << "createModelUseClosing=" << (createModelUseClosing ? 1 : 0) << '\n';
			ofs << "createModelClosingRadius=" << createModelClosingRadius << '\n';
			ofs << "createModelUseMean=" << (createModelUseMean ? 1 : 0) << '\n';
			ofs << "createModelMeanRadius=" << createModelMeanRadius << '\n';
			ofs << "angleStart=" << angleStart << '\n';
			ofs << "angleExtent=" << angleExtent << '\n';
			ofs << "contrast=" << contrast << '\n';
			ofs << "minContrast=" << minContrast << '\n';
			ofs << "minScore=" << minScore << '\n';
			ofs << "modelPath=" << modelPath << '\n';
			ofs << "stampPatternId=" << stampPatternId << '\n';
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
			double& createModelExposureTime2, double& createModelGain2,
			bool& upperLight, bool& lowerLight,
			bool& createModelUseOpening, int& createModelOpeningRadius,
			bool& createModelUseClosing, int& createModelClosingRadius,
			bool& createModelUseMean, int& createModelMeanRadius,
			double& angleStart, double& angleExtent,
			int& contrast, int& minContrast, double& minScore,
			std::string& modelPath,
			std::string& stampPatternId)
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
					else if (key == "createModelExposureTime2")
						createModelExposureTime2 = std::stod(value);
					else if (key == "createModelGain2")
						createModelGain2 = std::stod(value);
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
					else if (key == "angleStart")
						angleStart = std::stod(value);
					else if (key == "angleExtent")
						angleExtent = std::stod(value);
					else if (key == "contrast")
						contrast = std::stoi(value);
					else if (key == "minContrast")
						minContrast = std::stoi(value);
					else if (key == "minScore")
						minScore = std::stod(value);
					else if (key == "modelPath")
						modelPath = value;
					else if (key == "stampPatternId")
						stampPatternId = value;
				}
				catch (...)
				{
					continue;
				}
			}
			return true;
		}

		// 启动时调和残留的临时文件。残留形态（按数据新旧优先级）：
		// 1) x.jpg.tmp.jpg —— 旧版本把 x.jpg.tmp 传给 Halcon 被自动追加扩展名的产物
		//    （旧版 replaceFile 先删了正式文件、rename 又失败，此残留是唯一数据）；
		// 2) x.tmp.jpg     —— 现行 halconTmpPath 命名，崩溃在 write 与 rename 之间；
		// 3) x.jpg.tmp     —— 精确写名者（ofstream/WriteTuple）的残留。
		// 第一个能完整读出的收养为正式文件，其余删除；收养失败（被占用）保留待下次。
		void reconcileTmpFile(const fs::path& target,
			const std::function<bool(const fs::path&)>& validate)
		{
			std::vector<fs::path> candidates;
			const std::string ext = target.extension().string();
			if (!ext.empty())
			{
				candidates.push_back(target.string() + ".tmp" + ext);
				candidates.push_back(halconTmpPath(target));
			}
			candidates.push_back(target.string() + ".tmp");

			std::error_code ec;
			bool adopted = false;
			for (const auto& cand : candidates)
			{
				if (!fs::exists(cand, ec))
					continue;
				if (!adopted && validate(cand))
				{
					qWarning() << "[ShapeModelItem] 发现残留tmp，收养为正式文件:"
						<< QString::fromStdString(cand.string());
					if (replaceFile(cand, target))
						adopted = true;
					continue;
				}
				qWarning() << "[ShapeModelItem] 清理多余/损坏的tmp:"
					<< QString::fromStdString(cand.string());
				fs::remove(cand, ec);
			}
		}

		bool fileNonEmpty(const fs::path& p)
		{
			std::error_code ec;
			return fs::is_regular_file(p, ec) && fs::file_size(p, ec) > 0;
		}

		void unionRoiList(const std::vector<HalconCpp::HObject>& roiList, HalconCpp::HObject& outUnion, bool& outHasUnion)
		{
			outUnion.Clear();
			outHasUnion = false;
			HalconCpp::HObject merged;
			bool first = true;
			for (const auto& obj : roiList)
			{
				if (!obj.IsInitialized())
					continue;
				if (first)
				{
					merged = obj;
					first = false;
				}
				else
				{
					HalconCpp::HObject temp;
					HalconCpp::Union2(merged, obj, &temp);
					merged = temp;
				}
			}
			if (!first)
			{
				outUnion = merged;
				outHasUnion = true;
			}
		}
	}

	void ShapeModelData::buildRecognitionMask()
	{
		unionRoiList(_paintShieldRoiList, _recognitionMask, _hasRecognitionMask);
	}

	void ShapeModelData::loadInDir(const std::string& dir)
	{
		try
		{
			const fs::path dirPath(dir);

			// 加载前调和残留临时文件（恢复旧版本保存了但 rename 未生效的图像/参数）
			reconcileTmpFile(dirPath / kTemplateImageFile, [](const fs::path& p) {
				HalconCpp::HImage img; return readImageSafe(p, img); });
			reconcileTmpFile(dirPath / kOriginalImageFile, [](const fs::path& p) {
				HalconCpp::HImage img; return readImageSafe(p, img); });
			reconcileTmpFile(dirPath / kAnnotatedImageFile, [](const fs::path& p) {
				HalconCpp::HImage img; return readImageSafe(p, img); });
			reconcileTmpFile(dirPath / kParamsFile, fileNonEmpty);

			// 加载基本参数
			readParamsSafe(dirPath / kParamsFile,
				_createModelPreProcessType,
				centerX, centerY,
				findCenterX, findCenterY,
				offsetX, offsetY, offsetAngle,
				findnumber,
				_SingleChannelType,
				_createModelExposureTime, _createModelGain,
				_createModelExposureTime2, _createModelGain2,
				upperLight, lowerLight,
				_createModelUseOpening, _createModelOpeningRadius,
				_createModelUseClosing, _createModelClosingRadius,
				_createModelUseMean, _createModelMeanRadius,
				angleStart, angleExtent,
				contrast, minContrast, minScore,
				modelPath,
				stampPatternId);

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

			// 构建识别阶段使用的合并屏蔽区
			buildRecognitionMask();
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
				_createModelExposureTime2, _createModelGain2,
				upperLight, lowerLight,
				_createModelUseOpening, _createModelOpeningRadius,
				_createModelUseClosing, _createModelClosingRadius,
				_createModelUseMean, _createModelMeanRadius,
				angleStart, angleExtent,
				contrast, minContrast, minScore,
				modelPath,
				stampPatternId);

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

			reconcileTmpFile(dirPath / kModelInfoFile, [](const fs::path& p) {
				Json::Value v; return readJsonSafe(p, v); });

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
