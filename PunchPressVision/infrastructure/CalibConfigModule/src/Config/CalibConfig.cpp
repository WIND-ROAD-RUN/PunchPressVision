#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

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

			constexpr const char* kCameraParametersFile = "camera_parameters.tup";
			constexpr const char* kCameraPoseFile = "camera_pose.tup";
			constexpr const char* kSettingsFile = "camera_settings.txt";

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

			void writeSettingsSafe(const fs::path& filePath, double exposure, double gain)
			{
				fs::create_directories(filePath.parent_path());
				fs::path tmp = filePath;
				tmp += ".tmp";
				std::ofstream ofs(tmp);
				if (!ofs)
					return;
				ofs << "cameraExposure=" << exposure << '\n';
				ofs << "cameraGain=" << gain << '\n';
				ofs.close();
				replaceFile(tmp, filePath);
			}

			bool readSettingsSafe(const fs::path& filePath, double& exposure, double& gain)
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
						if (key == "cameraExposure")
							exposure = std::stod(value);
						else if (key == "cameraGain")
							gain = std::stod(value);
					}
					catch (...)
					{
						continue;
					}
				}
				return true;
			}
		}

		void CalibConfig::saveInDir(const std::string& dirPath)
		{
			try
			{
				const fs::path dir(dirPath);
				writeTupleSafe(dir / kCameraParametersFile, cameraParameters);
				writeTupleSafe(dir / kCameraPoseFile, cameraPose);
				writeSettingsSafe(dir / kSettingsFile, cameraExposure, cameraGain);
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
				const fs::path dir(dirPath);
				cameraExposure = 10000.0;
				cameraGain = 5.0;
				readTupleSafe(dir / kCameraParametersFile, cameraParameters);
				readTupleSafe(dir / kCameraPoseFile, cameraPose);
				readSettingsSafe(dir / kSettingsFile, cameraExposure, cameraGain);
			}
			catch (...)
			{
				// Ignore load errors; missing files keep the default values.
			}
		}
	}
}
