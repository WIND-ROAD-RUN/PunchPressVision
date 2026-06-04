#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace inf
{
	namespace Config
	{
		namespace
		{
			namespace fs = std::filesystem;

			constexpr const char* kCamera1ImageFile = "camera1_picture.bmp";
			constexpr const char* kCamera2ImageFile = "camera2_picture.bmp";
			constexpr const char* kParamsFile = "two_camera_splice_params.txt";
			constexpr const char* kCameraImageFormat = "bmp";

			std::string trimCr(const std::string& s)
			{
				if (!s.empty() && s.back() == '\r')
					return s.substr(0, s.size() - 1);
				return s;
			}

			void replaceFile(const fs::path& tmp, const fs::path& target)
			{
				if (fs::exists(target))
				{
					try { fs::remove(target); }
					catch (...) {}
				}
				fs::rename(tmp, target);
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
				tmp += ".tmp";
				HalconCpp::HImage(image).WriteImage(format, 0, tmp.string().c_str());
				replaceFile(tmp, filePath);
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

			void writeParamsSafe(const fs::path& filePath,
				const std::string& caltabPath,
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
				ofs << "DiffHeight=" << diffHeight << '\n';
				ofs << "OverlapInPercent=" << overlapPercent << '\n';
				ofs << "BorderInPercent=" << borderPercent << '\n';
				ofs << "DistancePlates=" << distancePlates << '\n';
				ofs << "pixTowWorld=" << pixToWorld << '\n';
				ofs << "rectifiedWidth=" << rectWidth << '\n';
				ofs << "rectifiedHeight=" << rectHeight << '\n';
				ofs.close();
				replaceFile(tmp, filePath);
			}

			bool readParamsSafe(const fs::path& filePath,
				std::string& caltabPath,
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
				writeParamsSafe(dir / kParamsFile,
					caltabDescrPath,
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
				DiffHeight = 0.0;
				OverlapInPercent = 0.0;
				BorderInPercent = 0.0;
				DistancePlates = 0.0;
				pixTowWorld = 0.0;
				rectifiedWidth = 0;
				rectifiedHeight = 0;
				readImageSafe(dir / kCamera1ImageFile, camera1Piccture);
				readImageSafe(dir / kCamera2ImageFile, camera2Piccture);
				readParamsSafe(dir / kParamsFile,
					caltabDescrPath,
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
}
