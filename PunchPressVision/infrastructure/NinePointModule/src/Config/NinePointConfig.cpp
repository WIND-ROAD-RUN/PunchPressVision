#include "infrastructure/NinePointModule/Config/NinePointConfig.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>

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

		constexpr const char* kHomMat2DFile = "hom_mat_2d.tup";
		constexpr const char* kParamsFile = "nine_point_params.txt";

		std::string trimCr(const std::string& s)
		{
			if (!s.empty() && s.back() == '\r')
				return s.substr(0, s.size() - 1);
			return s;
		}

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

			qWarning() << "[NinePointConfig] rename失败:"
				<< QString::fromStdString(tmp.string()) << "->"
				<< QString::fromStdString(target.string())
				<< "错误:" << QString::fromStdString(ec.message());

			fs::remove(target, ec);
			ec.clear();
			fs::rename(tmp, target, ec);
			if (ec)
			{
				qWarning() << "[NinePointConfig] 兜底rename仍失败:"
					<< QString::fromStdString(target.string())
					<< "错误:" << QString::fromStdString(ec.message());
			}
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
			double measureLength1, double measureLength2,
			double measureThreshold, double numMeasure,
			double cam1Exposure, double cam1Gain,
			double cam2Exposure, double cam2Gain,
			int xNumber, int yNumber, double distanceVal, double scaleVal,
			int xDiantance, int yDistance, double xOffset)
		{
			fs::create_directories(filePath.parent_path());
			fs::path tmp = filePath;
			tmp += ".tmp";
			std::ofstream ofs(tmp);
			if (!ofs)
				return;
			ofs << "MeasureLength1=" << measureLength1 << '\n';
			ofs << "MeasureLength2=" << measureLength2 << '\n';
			ofs << "MeasureThreshold=" << measureThreshold << '\n';
			ofs << "num_Measure=" << numMeasure << '\n';
			ofs << "camera1Exposure=" << cam1Exposure << '\n';
			ofs << "camera1Gain=" << cam1Gain << '\n';
			ofs << "camera2Exposure=" << cam2Exposure << '\n';
			ofs << "camera2Gain=" << cam2Gain << '\n';
			ofs << "xnumber=" << xNumber << '\n';
			ofs << "ynumber=" << yNumber << '\n';
			ofs << "distance=" << distanceVal << '\n';
			ofs << "scale=" << scaleVal << '\n';
			ofs << "xdiantance=" << xDiantance << '\n';
			ofs << "ydistance=" << yDistance << '\n';
			ofs << "xoffset=" << xOffset << '\n';
			ofs.close();
			replaceFile(tmp, filePath);
		}

		bool readParamsSafe(const fs::path& filePath,
			double& measureLength1, double& measureLength2,
			double& measureThreshold, double& numMeasure,
			double& cam1Exposure, double& cam1Gain,
			double& cam2Exposure, double& cam2Gain,
			int& xNumber, int& yNumber, double& distanceVal, double& scaleVal,
			int& xDiantance, int& yDistance, double& xOffset)
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
					if (key == "MeasureLength1")
						measureLength1 = std::stod(value);
					else if (key == "MeasureLength2")
						measureLength2 = std::stod(value);
					else if (key == "MeasureThreshold")
						measureThreshold = std::stod(value);
					else if (key == "num_Measure")
						numMeasure = std::stod(value);
					else if (key == "camera1Exposure")
						cam1Exposure = std::stod(value);
					else if (key == "camera1Gain")
						cam1Gain = std::stod(value);
					else if (key == "camera2Exposure")
						cam2Exposure = std::stod(value);
					else if (key == "camera2Gain")
						cam2Gain = std::stod(value);
					else if (key == "xnumber")
						xNumber = std::stoi(value);
					else if (key == "ynumber")
						yNumber = std::stoi(value);
					else if (key == "distance")
						distanceVal = std::stod(value);
					else if (key == "scale")
						scaleVal = std::stod(value);
					else if (key == "xdiantance")
						xDiantance = std::stoi(value);
					else if (key == "ydistance")
						yDistance = std::stoi(value);
					else if (key == "xoffset")
						xOffset = std::stod(value);
				}
				catch (...)
				{
					continue;
				}
			}
			return true;
		}
		// 启动时调和残留的 .tmp（旧版本 rename 静默失败/崩溃中断的产物）。
		// tmp 是每次保存先写的，必然不旧于正式文件：能完整读出则收养为正式文件
		//（恢复旧版本保存了但没生效的最新标定），损坏则删除。
		void reconcileTmpFile(const fs::path& target,
			const std::function<bool(const fs::path&)>& validate)
		{
			std::error_code ec;
			const fs::path tmp = target.string() + ".tmp";
			if (!fs::exists(tmp, ec))
				return;
			if (validate(tmp))
			{
				qWarning() << "[NinePointConfig] 发现残留tmp，收养为正式文件:"
					<< QString::fromStdString(tmp.string());
				replaceFile(tmp, target);
				return;
			}
			qWarning() << "[NinePointConfig] 发现损坏的tmp，删除:"
				<< QString::fromStdString(tmp.string());
			fs::remove(tmp, ec);
		}

		void reconcileTmpDir(const fs::path& dir)
		{
			std::error_code ec;
			if (!fs::is_directory(dir, ec))
				return;
			reconcileTmpFile(dir / kHomMat2DFile, [](const fs::path& p) {
				HalconCpp::HTuple t;
				return readTupleSafe(p, t) && t.Length() >= 6; });
			reconcileTmpFile(dir / kParamsFile, [](const fs::path& p) {
				double ml1, ml2, mt, nm, c1e, c1g, c2e, c2g, dist, scl, xo;
				int xn, yn, xd, yd;
				return readParamsSafe(p, ml1, ml2, mt, nm, c1e, c1g, c2e, c2g,
					xn, yn, dist, scl, xd, yd, xo); });
		}
	}

	void NinePointCfg::saveInDir(const std::string& dirPath)
	{
		try
		{
			const fs::path dir(dirPath);
			writeTupleSafe(dir / kHomMat2DFile, outHomMat2D);
			writeParamsSafe(dir / kParamsFile,
				MeasureLength1, MeasureLength2, MeasureThreshold, num_Measure,
				camera1Exposure, camera1Gain,
				camera2Exposure, camera2Gain,
				xnumber, ynumber, distance, scale,
				xdiantance, ydistance, xoffset);
		}
		catch (...)
		{
			// Ignore save errors to avoid crashing the application.
		}
	}

	void NinePointCfg::loadInDir(const std::string& dirPath)
	{
		try
		{
			const fs::path dir(dirPath);

			// 加载前先调和残留的 .tmp（主目录与 backup 都要处理），
			// 恢复旧版本保存了但 rename 未生效的最新标定
			reconcileTmpDir(dir);
			reconcileTmpDir(dir / kBackupDir);

			auto tryLoadTuple = [](const fs::path& file, HalconCpp::HTuple& tuple) -> bool
			{
				tuple.Clear();
				return readTupleSafe(file, tuple) && tuple.Length() >= 6;
			};

			auto tryLoadParams = [](const fs::path& file,
				double& ml1, double& ml2, double& mt, double& nm,
				double& c1e, double& c1g, double& c2e, double& c2g,
				int& xn, int& yn, double& dist, double& scl,
				int& xd, int& yd, double& xo) -> bool
			{
				return readParamsSafe(file, ml1, ml2, mt, nm, c1e, c1g, c2e, c2g,
					xn, yn, dist, scl, xd, yd, xo);
			};

			// 设置默认值
			MeasureLength1 = 100.0;
			MeasureLength2 = 50.0;
			MeasureThreshold = 1.0;
			num_Measure = 5.0;
			camera1Exposure = 5000.0;
			camera1Gain = 1.0;
			camera2Exposure = 5000.0;
			camera2Gain = 1.0;
			xnumber = 7;
			ynumber = 7;
			distance = 0.007;
			scale = 0.5;
			xdiantance = 500;
			ydistance = 100;
			xoffset = 400;

			outHomMat2D.Clear();

			// 先尝试主目录
			bool primaryOk = tryLoadTuple(dir / kHomMat2DFile, outHomMat2D);
			tryLoadParams(dir / kParamsFile,
				MeasureLength1, MeasureLength2, MeasureThreshold, num_Measure,
				camera1Exposure, camera1Gain, camera2Exposure, camera2Gain,
				xnumber, ynumber, distance, scale, xdiantance, ydistance, xoffset);

			if (primaryOk)
			{
				// 开机启动时加载成功 → 备份到 backup/ 供下次断电恢复
				const fs::path backupDir = dir / kBackupDir;
				backupFile(dir / kHomMat2DFile, backupDir);
				backupFile(dir / kParamsFile,   backupDir);
				return;
			}

			// 主文件损坏或缺失 → 从 backup/ 恢复
			const fs::path backupDir = dir / kBackupDir;
			HalconCpp::HTuple backupTuple;
			if (!tryLoadTuple(backupDir / kHomMat2DFile, backupTuple))
				return;

			// 恢复主参数
			outHomMat2D = backupTuple;
			tryLoadParams(backupDir / kParamsFile,
				MeasureLength1, MeasureLength2, MeasureThreshold, num_Measure,
				camera1Exposure, camera1Gain, camera2Exposure, camera2Gain,
				xnumber, ynumber, distance, scale, xdiantance, ydistance, xoffset);

			// 恢复后立即写回主目录
			saveInDir(dirPath);
		}
		catch (...)
		{
			// Ignore load errors; missing files keep the default values.
		}
	}
}
