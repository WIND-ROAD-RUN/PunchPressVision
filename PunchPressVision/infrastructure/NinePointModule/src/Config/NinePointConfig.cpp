#include "infrastructure/NinePointModule/Config/NinePointConfig.hpp"

#include <filesystem>
#include <fstream>
#include <string>

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

		void writeParamsSafe(const fs::path& filePath,
			double measureLength1, double measureLength2,
			double measureThreshold, double numMeasure,
			double cam1Exposure, double cam1Gain,
			double cam2Exposure, double cam2Gain,
			int xNumber, int yNumber, double distanceVal, double scaleVal,
			int xDiantance, int yDistance)
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
			ofs.close();
			replaceFile(tmp, filePath);
		}

		bool readParamsSafe(const fs::path& filePath,
			double& measureLength1, double& measureLength2,
			double& measureThreshold, double& numMeasure,
			double& cam1Exposure, double& cam1Gain,
			double& cam2Exposure, double& cam2Gain,
			int& xNumber, int& yNumber, double& distanceVal, double& scaleVal,
			int& xDiantance, int& yDistance)
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
				}
				catch (...)
				{
					continue;
				}
			}
			return true;
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
				xdiantance, ydistance);
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
			xdiantance = 400;
			ydistance = 100;
			readTupleSafe(dir / kHomMat2DFile, outHomMat2D);
			readParamsSafe(dir / kParamsFile,
				MeasureLength1, MeasureLength2, MeasureThreshold, num_Measure,
				camera1Exposure, camera1Gain,
				camera2Exposure, camera2Gain,
				xnumber, ynumber, distance, scale,
				xdiantance, ydistance);
		}
		catch (...)
		{
			// Ignore load errors; missing files keep the default values.
		}
	}
}
