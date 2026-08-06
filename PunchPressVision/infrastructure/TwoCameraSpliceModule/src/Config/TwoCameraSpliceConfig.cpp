#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

#include <filesystem>
#include <fstream>
#include <string>
namespace Config
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kCamera1ImageFile = "camera1_picture.bmp";
		constexpr const char* kCamera2ImageFile = "camera2_picture.bmp";
		constexpr const char* kMapSingle1File   = "map_single1.hobj";
		constexpr const char* kMapSingle2File   = "map_single2.hobj";
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
			// 先写 .tmp 再原子 rename，防止断电截断
			fs::path tmp = filePath;
			tmp += ".tmp";
			HalconCpp::HImage(image).WriteImage(format, 0, tmp.string().c_str());
			std::error_code ec;
			fs::rename(tmp, filePath, ec);
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
			// 先写 .tmp 再原子 rename，防止断电截断
			fs::path tmp = filePath;
			tmp += ".tmp";
			HalconCpp::WriteObject(obj, tmp.string().c_str());
			std::error_code ec;
			fs::rename(tmp, filePath, ec);
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
			// NTFS rename 原子替换，不先 remove 避免断电丢失窗口
			std::error_code ec;
			fs::rename(tmp, filePath, ec);
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

		constexpr const char* kBackupDir = "backup";

		// 将文件复制到备份目录（.tmp + rename 保证备份写入原子性）
		void backupFile(const fs::path& srcFile, const fs::path& backupDir)
		{
			std::error_code ec;
			fs::create_directories(backupDir, ec);
			if (ec) return;
			const fs::path dst = backupDir / srcFile.filename();
			const fs::path tmp = backupDir / (srcFile.filename().string() + ".tmp");
			fs::copy_file(srcFile, tmp, fs::copy_options::overwrite_existing, ec);
			if (!ec)
				fs::rename(tmp, dst, ec);
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

			auto tryLoadFromDir = [](const fs::path& d,
				HalconCpp::HObject& pic1, HalconCpp::HObject& pic2,
				HalconCpp::HObject& map1, HalconCpp::HObject& map2,
				std::string& caltab, double& c1g, double& c1e, double& c2g, double& c2e,
				double& dh, double& op, double& bp, double& dp, double& pw,
				int& rw, int& rh) -> bool
			{
				readImageSafe(d / kCamera1ImageFile, pic1);
				readImageSafe(d / kCamera2ImageFile, pic2);
				readObjectSafe(d / kMapSingle1File,  map1);
				readObjectSafe(d / kMapSingle2File,  map2);
				readParamsSafe(d / kParamsFile,
					caltab, c1g, c1e, c2g, c2e,
					dh, op, bp, dp, pw, rw, rh);
				return map1.IsInitialized();  // 核心判断：MapSingle1 就绪即整体就绪
			};

			caltabDescrPath.clear();
			camera1Gain = 0.0;
			camera1Exposure = 0.0;
			camera2Gain = 0.0;
			camera2Exposure = 0.0;
			DiffHeight = 0.0;
			OverlapInPercent = 0.0;
			BorderInPercent = 7.0;
			DistancePlates = 0.0;
			pixTowWorld = 0.0;
			rectifiedWidth = 0;
			rectifiedHeight = 0;

			// 先尝试主目录
			if (tryLoadFromDir(dir,
				camera1Piccture, camera2Piccture, MapSingle1, MapSingle2,
				caltabDescrPath, camera1Gain, camera1Exposure, camera2Gain, camera2Exposure,
				DiffHeight, OverlapInPercent, BorderInPercent, DistancePlates,
				pixTowWorld, rectifiedWidth, rectifiedHeight))
			{
				// 开机启动时加载成功 → 备份到 backup/ 供下次断电恢复
				const fs::path bkDir = dir / kBackupDir;
				backupFile(dir / kCamera1ImageFile, bkDir);
				backupFile(dir / kCamera2ImageFile, bkDir);
				backupFile(dir / kMapSingle1File,   bkDir);
				backupFile(dir / kMapSingle2File,   bkDir);
				backupFile(dir / kParamsFile,       bkDir);
				return;
			}

			// 主文件损坏或缺失 → 从 backup/ 恢复
			const fs::path backupDir = dir / kBackupDir;
			if (tryLoadFromDir(backupDir,
				camera1Piccture, camera2Piccture, MapSingle1, MapSingle2,
				caltabDescrPath, camera1Gain, camera1Exposure, camera2Gain, camera2Exposure,
				DiffHeight, OverlapInPercent, BorderInPercent, DistancePlates,
				pixTowWorld, rectifiedWidth, rectifiedHeight))
			{
				// 恢复后立即写回主目录
				saveInDir(dirPath);
			}
		}
		catch (...)
		{
			// Ignore load errors; missing files keep the default values.
		}
	}
}
