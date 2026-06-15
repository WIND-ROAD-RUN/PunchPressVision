#include "infTool/CalibInfTool/CalibInfTool.hpp"

#include <algorithm>

#include <QObject>

namespace infTool
{
	namespace PRIVATE
	{
		HalconCpp::HImage cvMatToHImage(const cv::Mat& mat)
		{
			// 检查输入 cv::Mat 是否为空
			if (mat.empty())
			{
				throw std::invalid_argument("Input cv::Mat is empty.");
			}

			// 获取 cv::Mat 的宽度、高度和通道数
			int width = mat.cols;
			int height = mat.rows;
			int channels = mat.channels();

			HalconCpp::HImage hImage;

			// 根据 cv::Mat 的通道数生成对应的 HImage
			if (channels == 1)
			{
				// 单通道灰度图像
				hImage.GenImage1("byte", width, height, const_cast<void*>(static_cast<const void*>(mat.data)));
			}
			else if (channels == 3)
			{
				// 三通道彩色图像，OpenCV 默认存储顺序为 BGR
				hImage.GenImageInterleaved(
					const_cast<void*>(static_cast<const void*>(mat.data)), // PixelPointer
					"bgr",                                                // ColorFormat
					width,                                                 // OriginalWidth
					height,                                                // OriginalHeight
					0,                                                     // Alignment
					"byte",                                                // Type
					width,                                                 // ImageWidth
					height,                                                // ImageHeight
					0,                                                     // StartRow
					0,                                                     // StartColumn
					8,                                                     // BitsPerChannel
					0                                                      // BitShift
				);
			}
			else
			{
				throw std::invalid_argument("Unsupported cv::Mat format. Only 1-channel and 3-channel images are supported.");
			}

			return hImage;
		}
	}

	CalibInfTool::CalibInfTool(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void CalibInfTool::calibCamera(const std::vector<HalconCpp::HImage>& himages,
		Config::CalibConfigItem& item)
	{
		std::string err;
		calibrateFromImages(himages, /*focalLengthMm*/ 8.0, /*plateThicknessMm*/ 0.0,
			item, /*referenceIndex*/ 0, &err);
	}
	
	bool CalibInfTool::calibrateFromImages(const std::vector<HalconCpp::HImage>& himages,
		double focalLengthMm,
		double plateThicknessMm,
		Config::CalibConfigItem& item,
		int referenceIndex,
		std::string* errorMsg)
	{
		using namespace HalconCpp;

		// 基础校验
		if (himages.empty())
		{
			if (errorMsg) *errorMsg = "图像列表为空";
			return false;
		}
		if (item.calibBoardDescrPath.empty())
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
			SetCalibDataCalibObject(hv_CalibHandle, 0, item.calibBoardDescrPath.c_str());

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

			// 写入 CalibConfigItem
			item.cameraParameters = hv_CameraParameters;
			item.cameraPose = hv_CameraPose;
			item.calibrationErrors = hv_Errors;
			item.calibrationReferenceIndex = referenceIndex;

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
	//测试发现，FindCalibObject 成功时会返回标记点坐标，但如果没有找到标定板，则不会抛出异常，而是返回空坐标。因此在后续获取坐标时需要检查是否成功找到标定板。
	bool CalibInfTool::drawCalibMarks(const HalconCpp::HImage& src,
		Config::CalibConfigItem& item,
		bool& isOk,
		HalconCpp::HObject& outMarksXld,
		HalconCpp::HObject& outMarksRegion,
		std::string* errorMsg)
	{
		using namespace HalconCpp;

		isOk = false;

		GenEmptyObj(&outMarksXld);
		GenEmptyObj(&outMarksRegion);

		if (!src.IsInitialized())
		{
			if (errorMsg) *errorMsg = "输入图像未初始化";
			return false;
		}

		if (item.calibBoardDescrPath.empty())
		{
			if (errorMsg) *errorMsg = "标定板描述文件路径为空";
			return false;
		}

		HTuple hv_W, hv_H;
		try
		{
			src.GetImageSize(&hv_W, &hv_H);
		}
		catch (const HException&)
		{
			if (errorMsg) *errorMsg = "获取图像尺寸失败";
			return false;
		}

		HTuple StartParameters;
		StartParameters[0] = "area_scan_polynomial";
		StartParameters[1] = 0.008;
		StartParameters[2] = 0;
		StartParameters[3] = 0;
		StartParameters[4] = 0;
		StartParameters[5] = 0;
		StartParameters[6] = 0;
		StartParameters[7] = 2.4e-06;
		StartParameters[8] = 2.4e-06;
		StartParameters[9] = hv_W[0].D() / 2.0;
		StartParameters[10] = hv_H[0].D() / 2.0;
		StartParameters[11] = hv_W;
		StartParameters[12] = hv_H;

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
			CreateCalibData("calibration_object", 1, 1, &hv_CalibHandle);
			SetCalibDataCamParam(hv_CalibHandle, 0, HTuple(), StartParameters);
			SetCalibDataCalibObject(hv_CalibHandle, 0, item.calibBoardDescrPath.c_str());

			// 转灰度再找板（更稳）
			HImage hGray;
			HTuple hv_Channels;
			CountChannels(src, &hv_Channels);
			if (hv_Channels.TupleLength() > 0 && hv_Channels[0].I() == 3)
				Rgb1ToGray(src, &hGray);
			else
				hGray = src;

			FindCalibObject(hGray, hv_CalibHandle, 0, 0, 0, hv_FindCalObjParNames, hv_FindCalObjParValues);

			HTuple hv_MarkRows, hv_MarkColumns, hv_Ind, hv_CameraPose;
			GetCalibDataObservPoints(hv_CalibHandle, 0, 0, 0, &hv_MarkRows, &hv_MarkColumns, &hv_Ind, &hv_CameraPose);

			if (hv_MarkRows.TupleLength() == 0 || hv_MarkColumns.TupleLength() == 0)
			{
				isOk = false;
				ClearCalibData(hv_CalibHandle);
				if (errorMsg) *errorMsg = "未找到标定板标记点";
				return false;
			}

			// 输出 XLD：每个点十字（保留，便于看到原点位置）
			GenCrossContourXld(&outMarksXld, hv_MarkRows, hv_MarkColumns, 12, 0.785398);

			// 注意：这里不再生成整体外接矩形区域（避免在显示时叠加一个大矩形）。
			// outMarksRegion 保持为空对象即可。

			isOk = true;
			ClearCalibData(hv_CalibHandle);
			return true;
		}
		catch (const HException& except)
		{
			try { ClearCalibData(hv_CalibHandle); }
			catch (...) {}
			isOk = false;
			if (errorMsg) *errorMsg = std::string("查找标定板标记异常: ") + except.ErrorMessage().Text();
			return false;
		}
	}

	HalconCpp::HImage CalibInfTool::undistortImage(const HalconCpp::HImage& himage,
		global::CameraIndex cameraIndex)
	{
		using namespace HalconCpp;
		auto& item = inf_.calib_config_module_->calibConfig.item(cameraIndex);

		// 未标定或输入无效时原样返回，确保流水线不中断
		if (!himage.IsInitialized() || item.cameraParameters.Length() == 0)
			return himage;

		try
		{
			// 生成无畸变（理想）相机参数
			HTuple camParRectified;
			ChangeRadialDistortionCamPar("fixed", item.cameraParameters, 0, &camParRectified);

			// 执行畸变矫正（Region 传空对象表示对整图处理）
			HObject rectified;
			ChangeRadialDistortionImage(himage, HObject(), &rectified,
				item.cameraParameters, camParRectified);
			return HImage(rectified);
		}
		catch (const HException&)
		{
			return himage;
		}
	}






	void CalibInfTool::onCameraFrame(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
	{
		if (!matInfo.mat.empty())
		{
			// TODO:
			
			auto hImage = PRIVATE::cvMatToHImage(matInfo.mat);




			emit callBackFunc(hImage, cameraIndex);
		}

	}

	void CalibInfTool::build()
	{
		// 桥接 CameraModule 的原始帧信号，经 onCameraFrame 处理后转发
		if (inf_.camera_module_)
		{
			QObject::connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
				this, &CalibInfTool::onCameraFrame, Qt::DirectConnection);
		}
	}

	void CalibInfTool::destroy()
	{
		if (inf_.camera_module_)
		{
			QObject::disconnect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
				this, &CalibInfTool::onCameraFrame);
		}
	}

	void CalibInfTool::start()
	{
	}

	void CalibInfTool::stop()
	{
	}
}
