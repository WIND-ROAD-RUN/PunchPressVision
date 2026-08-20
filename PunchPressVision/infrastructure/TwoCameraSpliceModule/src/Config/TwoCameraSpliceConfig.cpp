#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

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

		// Windows 上 fs::rename 在目标已存在时会失败（ERROR_ALREADY_EXISTS），
		// 不具备 POSIX 的原子替换语义；失败时退回 MoveFileExW 强制替换，
		// 再兜底 remove+rename，避免 .tmp 残留、新参数永远不生效。
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

			qWarning() << "[TwoCameraSpliceCfg] rename失败:"
				<< QString::fromStdString(tmp.string()) << "->"
				<< QString::fromStdString(target.string())
				<< "错误:" << QString::fromStdString(ec.message());

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
			if (ec)
			{
				qWarning() << "[TwoCameraSpliceCfg] 兜底rename仍失败:"
					<< QString::fromStdString(target.string())
					<< "错误:" << QString::fromStdString(ec.message());
				return false;
			}
			return true;
		}

		constexpr const char* kCamera1ImageFile = "camera1_picture.bmp";
		constexpr const char* kCamera2ImageFile = "camera2_picture.bmp";
		constexpr const char* kMapSingle1File   = "map_single1.hobj";
		constexpr const char* kMapSingle2File   = "map_single2.hobj";
		constexpr const char* kParamsFile = "two_camera_splice_params.txt";
		constexpr const char* kCameraImageFormat = "bmp";

		// 实测 Halcon 24.11 的扩展名规则：WriteImage/WriteObject 在文件名
		// 不以"该格式可识别的扩展名"结尾时会自动追加（x.bmp.tmp 会被写成
		// x.bmp.tmp.bmp，x.hobj.tmp 会被写成 x.hobj.tmp.hobj）。
		// 因此给 Halcon 的临时文件名必须把 .tmp 插在扩展名之前
		// （camera1_picture.tmp.bmp），保证实际写出的文件名可预测，
		// 后续 replaceFile 才能找到它。
		fs::path halconTmpPath(const fs::path& target)
		{
			return target.parent_path() /
				(target.stem().string() + ".tmp" + target.extension().string());
		}

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
			// 先写临时文件再原子 rename，防止断电截断。
			// 注意必须用 halconTmpPath：直接传 x.bmp.tmp 会被 Halcon
			// 追加扩展名写成 x.bmp.tmp.bmp，导致 rename 找不到源文件。
			const fs::path tmp = halconTmpPath(filePath);
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
			// 先写临时文件再原子 rename，防止断电截断（同上用 halconTmpPath，
			// 否则 WriteObject 会把 x.hobj.tmp 写成 x.hobj.tmp.hobj）
			const fs::path tmp = halconTmpPath(filePath);
			HalconCpp::WriteObject(obj, tmp.string().c_str());
			replaceFile(tmp, filePath);
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
			replaceFile(tmp, filePath);
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

		// 启动时调和残留的临时文件（上次保存中断或旧版本 bug 的产物）。
		// 可能存在的残留形态（按数据新旧优先级）：
		// 1) x.bmp.tmp.bmp —— 旧版本把 x.bmp.tmp 传给 Halcon 被自动追加扩展名的产物，
		//    内容是旧版本每次保存写入的最新完整数据（rename 因源名不符从未生效）；
		// 2) x.tmp.bmp     —— 现行 halconTmpPath 命名，崩溃在 write 与 rename 之间的残留；
		// 3) x.bmp.tmp     —— 精确写名者（ofstream/WriteTuple/copy_file）的残留。
		// 策略：第一个能完整读出的残留收养为正式文件（它一定不旧于正式文件），
		// 其余残留（损坏或重复）删除；收养失败（被占用）则保留待下次启动。
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
					qWarning() << "[TwoCameraSpliceCfg] 发现残留tmp，收养为正式文件:"
						<< QString::fromStdString(cand.string());
					if (replaceFile(cand, target))
						adopted = true;
					else
						qWarning() << "[TwoCameraSpliceCfg] tmp收养失败，保留待下次启动处理:"
							<< QString::fromStdString(cand.string());
					continue;
				}
				qWarning() << "[TwoCameraSpliceCfg] 清理多余/损坏的tmp:"
					<< QString::fromStdString(cand.string());
				fs::remove(cand, ec);
			}
		}

		void reconcileTmpDir(const fs::path& dir)
		{
			std::error_code ec;
			if (!fs::is_directory(dir, ec))
				return;
			reconcileTmpFile(dir / kCamera1ImageFile, [](const fs::path& p) {
				HalconCpp::HObject img; return readImageSafe(p, img); });
			reconcileTmpFile(dir / kCamera2ImageFile, [](const fs::path& p) {
				HalconCpp::HObject img; return readImageSafe(p, img); });
			reconcileTmpFile(dir / kMapSingle1File, [](const fs::path& p) {
				HalconCpp::HObject obj; return readObjectSafe(p, obj); });
			reconcileTmpFile(dir / kMapSingle2File, [](const fs::path& p) {
				HalconCpp::HObject obj; return readObjectSafe(p, obj); });
			reconcileTmpFile(dir / kParamsFile, [](const fs::path& p) {
				std::string caltab; double c1g, c1e, c2g, c2e, dh, op, bp, dp, pw;
				int rw, rh;
				return readParamsSafe(p, caltab, c1g, c1e, c2g, c2e,
					dh, op, bp, dp, pw, rw, rh); });
		}

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
				replaceFile(tmp, dst);
		}
	}

	void TwoCameraSpliceCfg::saveInDir(const std::string& dirPath)
	{
		qDebug() << "[TwoCameraSpliceCfg] 保存拼接参数, 目录:" << QString::fromStdString(dirPath)
			<< "| map1Init=" << MapSingle1.IsInitialized()
			<< "map2Init=" << MapSingle2.IsInitialized()
			<< "cam1ImgInit=" << camera1Piccture.IsInitialized()
			<< "cam2ImgInit=" << camera2Piccture.IsInitialized()
			<< "| caltab=" << QString::fromStdString(caltabDescrPath)
			<< "pixTowWorld=" << pixTowWorld
			<< "rectified=" << rectifiedWidth << "x" << rectifiedHeight;
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
			qDebug() << "[TwoCameraSpliceCfg] 保存完成:" << QString::fromStdString(dirPath);
		}
		catch (const std::exception& e)
		{
			qWarning() << "[TwoCameraSpliceCfg] 保存异常:" << e.what();
		}
		catch (...)
		{
			qWarning() << "[TwoCameraSpliceCfg] 保存发生未知异常";
		}
	}

	void TwoCameraSpliceCfg::loadInDir(const std::string& dirPath)
	{
		qDebug() << "[TwoCameraSpliceCfg] 加载拼接参数, 目录:" << QString::fromStdString(dirPath);
		try
		{
			const fs::path dir(dirPath);

			// 加载前先调和残留的 .tmp（主目录与 backup 都要处理），
			// 保证后续读到的是最新且完整的数据
			reconcileTmpDir(dir);
			reconcileTmpDir(dir / kBackupDir);

			auto tryLoadFromDir = [](const fs::path& d,
				HalconCpp::HObject& pic1, HalconCpp::HObject& pic2,
				HalconCpp::HObject& map1, HalconCpp::HObject& map2,
				std::string& caltab, double& c1g, double& c1e, double& c2g, double& c2e,
				double& dh, double& op, double& bp, double& dp, double& pw,
				int& rw, int& rh) -> bool
			{
				const bool img1Ok  = readImageSafe(d / kCamera1ImageFile, pic1);
				const bool img2Ok  = readImageSafe(d / kCamera2ImageFile, pic2);
				const bool map1Ok  = readObjectSafe(d / kMapSingle1File,  map1);
				const bool map2Ok  = readObjectSafe(d / kMapSingle2File,  map2);
				const bool paramsOk = readParamsSafe(d / kParamsFile,
					caltab, c1g, c1e, c2g, c2e,
					dh, op, bp, dp, pw, rw, rh);
				qDebug() << "[TwoCameraSpliceCfg] 尝试从" << QString::fromStdString(d.string()) << "加载:"
					<< "cam1图=" << img1Ok << "cam2图=" << img2Ok
					<< "map1=" << map1Ok << "map2=" << map2Ok
					<< "params=" << paramsOk
					<< "| caltab=" << QString::fromStdString(caltab)
					<< "cam1Gain=" << c1g << "cam1Exposure=" << c1e
					<< "cam2Gain=" << c2g << "cam2Exposure=" << c2e
					<< "DiffHeight=" << dh << "Overlap%=" << op
					<< "Border%=" << bp << "DistancePlates=" << dp
					<< "pixTowWorld=" << pw
					<< "rectified=" << rw << "x" << rh;
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
				qDebug() << "[TwoCameraSpliceCfg] 主目录加载成功，备份到backup/";
				// 开机启动时加载成功 → 备份到 backup/ 供下次断电恢复
				const fs::path bkDir = dir / kBackupDir;
				backupFile(dir / kCamera1ImageFile, bkDir);
				backupFile(dir / kCamera2ImageFile, bkDir);
				backupFile(dir / kMapSingle1File,   bkDir);
				backupFile(dir / kMapSingle2File,   bkDir);
				backupFile(dir / kParamsFile,       bkDir);
				return;
			}

			qWarning() << "[TwoCameraSpliceCfg] 主目录加载失败（map1未就绪），尝试从backup/恢复";
			// 主文件损坏或缺失 → 从 backup/ 恢复
			const fs::path backupDir = dir / kBackupDir;
			if (tryLoadFromDir(backupDir,
				camera1Piccture, camera2Piccture, MapSingle1, MapSingle2,
				caltabDescrPath, camera1Gain, camera1Exposure, camera2Gain, camera2Exposure,
				DiffHeight, OverlapInPercent, BorderInPercent, DistancePlates,
				pixTowWorld, rectifiedWidth, rectifiedHeight))
			{
				qDebug() << "[TwoCameraSpliceCfg] 从backup恢复成功，写回主目录";
				// 恢复后立即写回主目录
				saveInDir(dirPath);
			}
			else
			{
				qWarning() << "[TwoCameraSpliceCfg] 无可用拼接参数：主目录与backup均缺失或损坏";
			}
		}
		catch (const std::exception& e)
		{
			qWarning() << "[TwoCameraSpliceCfg] 加载异常:" << e.what();
		}
		catch (...)
		{
			qWarning() << "[TwoCameraSpliceCfg] 加载发生未知异常";
		}
	}
}
