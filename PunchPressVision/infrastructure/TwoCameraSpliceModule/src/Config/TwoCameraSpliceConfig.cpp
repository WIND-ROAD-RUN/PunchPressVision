#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

#include <filesystem>
#include <fstream>
#include <string>
namespace Config
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kCamera1ImageFile = "camera1_picture";
		constexpr const char* kCamera2ImageFile = "camera2_picture";
		constexpr const char* kMapSingle1File   = "map_single1";
		constexpr const char* kMapSingle2File   = "map_single2";
		constexpr const char* kParamsFile = "two_camera_splice_params.txt";
		constexpr const char* kCameraImageFormat = "bmp";

		std::string trimCr(const std::string& s)
		{
			if (!s.empty() && s.back() == '\r')
				return s.substr(0, s.size() - 1);
			return s;
		}

	

		void writeImageSafe(const HalconCpp::HObject& image, const fs::path& filePath, const char* format)
		{
			fs::create_directories(filePath.parent_path());
			if (!image.IsInitialized())
			{
				try
				{
					if (fs::exists(filePath))
						fs::remove(filePath);
				}
				catch (...) {}
				return;
			}
			fs::path tmp = filePath;
			
			HalconCpp::HImage(image).WriteImage(format, 0, tmp.string().c_str());
		}

		bool readImageSafe(const fs::path& filePath, HalconCpp::HObject& image)
		{
			if (!fs::exists(filePath))
				return false;
			try
			{
				HalconCpp::HImage tmpImg;
				tmpImg.ReadImage(filePath.string().c_str());
				image = tmpImg;
			}
			catch (...)
			{
				return false;
			}
			return true;
		}

		// Map 对象（多通道映射图）不能走 WriteImage/ReadImage（BMP 通道数不足），
		// 必须用 Halcon 原生 WriteObject/ReadObject 序列化。
		void writeObjectSafe(const HalconCpp::HObject& obj, const fs::path& filePath)
		{
			fs::create_directories(filePath.parent_path());
			if (!obj.IsInitialized())
			{
				try { if (fs::exists(filePath)) fs::remove(filePath); }
				catch (...) {}
				return;
			}
			fs::path tmp = filePath;
			
			HalconCpp::WriteObject(obj, tmp.string().c_str());
		}

		bool readObjectSafe(const fs::path& filePath, HalconCpp::HObject& obj)
		{
			if (!fs::exists(filePath))
				return false;
			try
			{
				HalconCpp::HObject tmpObj;
				HalconCpp::ReadObject(&tmpObj, filePath.string().c_str());
				obj = tmpObj;
			}
			catch (...)
			{
				return false;
			}
			return true;
		}

		void writeParamsSafe(const fs::path& filePath,
			const std::string& caltabPath,
			double cam1Gain, double cam1Exposure,
			double cam2Gain, double cam2Exposure,
			double diffHeight, double overlapPercent,
			double borderPercent, double distancePlates,
			double pixToWorld,
			int rectWidth, int rectHeight)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";
			std::ofstream ofs(tmp);
			if (!ofs)
				return;
			ofs << "caltabDescrPath=" << caltabPath << '\n';
			ofs << "camera1Gain=" << cam1Gain << '\n';
			ofs << "camera1Exposure=" << cam1Exposure << '\n';
			ofs << "camera2Gain=" << cam2Gain << '\n';
			ofs << "camera2Exposure=" << cam2Exposure << '\n';
			ofs << "DiffHeight=" << diffHeight << '\n';
			ofs << "OverlapInPercent=" << overlapPercent << '\n';
			ofs << "BorderInPercent=" << borderPercent << '\n';
			ofs << "DistancePlates=" << distancePlates << '\n';
			ofs << "pixTowWorld=" << pixToWorld << '\n';
			ofs << "rectifiedWidth=" << rectWidth << '\n';
			ofs << "rectifiedHeight=" << rectHeight << '\n';
			ofs.close();
		}

		bool readParamsSafe(const fs::path& filePath,
			std::string& caltabPath,
			double& cam1Gain, double& cam1Exposure,
			double& cam2Gain, double& cam2Exposure,
			double& diffHeight, double& overlapPercent,
			double& borderPercent, double& distancePlates,
			double& pixToWorld,
			int& rectWidth, int& rectHeight)
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
					if (key == "caltabDescrPath")
						caltabPath = value;
					else if (key == "camera1Gain")
						cam1Gain = std::stod(value);
					else if (key == "camera1Exposure")
						cam1Exposure = std::stod(value);
					else if (key == "camera2Gain")
						cam2Gain = std::stod(value);
					else if (key == "camera2Exposure")
						cam2Exposure = std::stod(value);
					else if (key == "DiffHeight")
						diffHeight = std::stod(value);
					else if (key == "OverlapInPercent")
						overlapPercent = std::stod(value);
					else if (key == "BorderInPercent")
						borderPercent = std::stod(value);
					else if (key == "DistancePlates")
						distancePlates = std::stod(value);
					else if (key == "pixTowWorld")
						pixToWorld = std::stod(value);
					else if (key == "rectifiedWidth")
						rectWidth = std::stoi(value);
					else if (key == "rectifiedHeight")
						rectHeight = std::stoi(value);
				}
				catch (...)
				{
					continue;
				}
			}
			return true;
		}
	}

	void TwoCameraSpliceCfg::saveInDir(const std::string& dirPath)
	{
		try
		{
			const fs::path dir(dirPath);
			writeImageSafe(camera1Piccture, dir / kCamera1ImageFile, kCameraImageFormat);
			writeImageSafe(camera2Piccture, dir / kCamera2ImageFile, kCameraImageFormat);
			writeObjectSafe(MapSingle1,     dir / kMapSingle1File);
			writeObjectSafe(MapSingle2,     dir / kMapSingle2File);
			writeParamsSafe(dir / kParamsFile,
				caltabDescrPath,
				camera1Gain, camera1Exposure,
				camera2Gain, camera2Exposure,
				DiffHeight, OverlapInPercent, BorderInPercent, DistancePlates,
				pixTowWorld,
				rectifiedWidth, rectifiedHeight);
		}
		catch (...)
		{
			// Ignore save errors to avoid crashing the application.
		}
	}

	void TwoCameraSpliceCfg::loadInDir(const std::string& dirPath)
	{
		try
		{
			const fs::path dir(dirPath);
			caltabDescrPath.clear();
			camera1Gain = 0.0;
			camera1Exposure = 0.0;
			camera2Gain = 0.0;
			camera2Exposure = 0.0;
			DiffHeight = 0.0;
			OverlapInPercent = 0.0;
			BorderInPercent = 0.0;
			DistancePlates = 0.0;
			pixTowWorld = 0.0;
			rectifiedWidth = 0;
			rectifiedHeight = 0;
			readImageSafe(dir / kCamera1ImageFile, camera1Piccture);
			readImageSafe(dir / kCamera2ImageFile, camera2Piccture);
			readObjectSafe(dir / kMapSingle1File,  MapSingle1);
			readObjectSafe(dir / kMapSingle2File,  MapSingle2);
			readParamsSafe(dir / kParamsFile,
				caltabDescrPath,
				camera1Gain, camera1Exposure,
				camera2Gain, camera2Exposure,
				DiffHeight, OverlapInPercent, BorderInPercent, DistancePlates,
				pixTowWorld,
				rectifiedWidth, rectifiedHeight);
		}
		catch (...)
		{
			// Ignore load errors; missing files keep the default values.
		}
	}
}
