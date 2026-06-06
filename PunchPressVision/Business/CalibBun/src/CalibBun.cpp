#include "Business/CalibBun/CalibBun.hpp"

#include <algorithm>

namespace bun
{
	CalibBun::CalibBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void CalibBun::calibCamera(const std::vector<HalconCpp::HImage>& himages)
	{
		Config::CalibConfig calibConfig;
		//TODO:实现通过himages标定相机畸变矫正结果

		inf_.calib_config_module_->calibConfig = calibConfig;
	}

	bool CalibBun::calibrateFromImages(const std::vector<HalconCpp::HImage>& himages,
		double focalLengthMm,
		double plateThicknessMm,
		int referenceIndex,
		std::string* errorMsg)
	{
		using namespace HalconCpp;
		auto& cfg = inf_.calib_config_module_->calibConfig;

		// 基础校验
		if (himages.empty())
		{
			if (errorMsg) *errorMsg = "图像列表为空";
			return false;
		}
		if (cfg.calibBoardDescrPath.empty())
		{
			if (errorMsg) *errorMsg = "标定板描述文件路径为空";
			return false;
		}

		// 找第一张有效图，用于取图像尺寸
		const HImage* first = nullptr;
		for (const auto& img : himages)
		{
			if (img.IsInitialized())
			{
				first = &img;
				break;
			}
		}
		if (first == nullptr)
		{
			if (errorMsg) *errorMsg = "没有有效的图像";
			return false;
		}

		HTuple hv_W, hv_H;
		try
		{
			first->GetImageSize(&hv_W, &hv_H);
		}
		catch (const HException&)
		{
			if (errorMsg) *errorMsg = "获取图像尺寸失败";
			return false;
		}

		// StartParameters：与 HDevelop 脚本一致
		HTuple hv_StartParameters;
		hv_StartParameters[0] = "area_scan_polynomial";
		hv_StartParameters[1] = focalLengthMm / 1000.0;
		hv_StartParameters[2] = 0;
		hv_StartParameters[3] = 0;
		hv_StartParameters[4] = 0;
		hv_StartParameters[5] = 0;
		hv_StartParameters[6] = 0;
		hv_StartParameters[7] = 2.4e-06;
		hv_StartParameters[8] = 2.4e-06;
		hv_StartParameters[9] = hv_W[0].D() / 2.0;
		hv_StartParameters[10] = hv_H[0].D() / 2.0;
		hv_StartParameters[11] = hv_W;
		hv_StartParameters[12] = hv_H;

		HTuple hv_FindCalObjParNames, hv_FindCalObjParValues;
		hv_FindCalObjParNames[0] = "gap_tolerance";
		hv_FindCalObjParValues[0] = 1;
		hv_FindCalObjParNames[1] = "alpha";
		hv_FindCalObjParValues[1] = 1;
		hv_FindCalObjParNames[2] = "skip_find_caltab";
		hv_FindCalObjParValues[2] = "false";

		HTuple hv_CalibHandle;
		try
		{
			// 创建 CalibData 并设置相机初始参数与标定对象描述
			CreateCalibData("calibration_object", 1, 1, &hv_CalibHandle);
			SetCalibDataCamParam(hv_CalibHandle, 0, HTuple(), hv_StartParameters);
			SetCalibDataCalibObject(hv_CalibHandle, 0, cfg.calibBoardDescrPath.c_str());

			// 为每张有效图像执行 FindCalibObject（观测 index 连续递增）
			int usedIndex = 0;
			for (size_t i = 0; i < himages.size(); ++i)
			{
				const auto& img = himages[i];
				if (!img.IsInitialized())
					continue;

				// 灰度化（如果是彩色）
				HImage hGray;
				HTuple hv_Channels;
				CountChannels(img, &hv_Channels);
				if (hv_Channels.TupleLength() > 0 && hv_Channels[0].I() == 3)
					Rgb1ToGray(img, &hGray);
				else
					hGray = img;

				FindCalibObject(hGray, hv_CalibHandle, 0, 0, usedIndex, hv_FindCalObjParNames, hv_FindCalObjParValues);
				++usedIndex;
			}

			if (usedIndex <= 0)
			{
				ClearCalibData(hv_CalibHandle);
				if (errorMsg) *errorMsg = "没有一张图像成功找到标定板";
				return false;
			}

			// 执行实际标定
			HTuple hv_Errors;
			CalibrateCameras(hv_CalibHandle, &hv_Errors);

			// 获取标定后的相机参数和参考位姿
			HTuple hv_CameraParameters;
			GetCalibData(hv_CalibHandle, "camera", 0, "params", &hv_CameraParameters);

			referenceIndex = std::clamp(referenceIndex, 0, usedIndex - 1);

			HTuple hv_CameraPose;
			GetCalibData(hv_CalibHandle, "calib_obj_pose", HTuple(0).TupleConcat(referenceIndex), "pose", &hv_CameraPose);

			// 根据板厚调整原点（与 HDevelop 一致）
			SetOriginPose(hv_CameraPose, 0.0, 0.0, plateThicknessMm / 1000.0, &hv_CameraPose);

			// 写入 CalibConfig
			cfg.cameraParameters = hv_CameraParameters;
			cfg.cameraPose = hv_CameraPose;
			cfg.calibrationErrors = hv_Errors;
			cfg.calibrationReferenceIndex = referenceIndex;

			ClearCalibData(hv_CalibHandle);
			return true;
		}
		catch (const HalconCpp::HException& except)
		{
			try { ClearCalibData(hv_CalibHandle); }
			catch (...) {}
			if (errorMsg) *errorMsg = std::string("标定异常: ") + except.ErrorMessage().Text();
			return false;
		}
	}

	HalconCpp::HImage CalibBun::undistortImage(const HalconCpp::HImage& himage)
	{
		HalconCpp::HImage result;
		auto& cfg = inf_.calib_config_module_->calibConfig;
		//TODO:实现通过cfg中的相机内参和外参对himage进行畸变矫正，并将结果保存在result中

		return result;
	}






	void CalibBun::build()
	{
	}

	void CalibBun::destroy()
	{
	}

	void CalibBun::start()
	{
	}

	void CalibBun::stop()
	{
	}
}
