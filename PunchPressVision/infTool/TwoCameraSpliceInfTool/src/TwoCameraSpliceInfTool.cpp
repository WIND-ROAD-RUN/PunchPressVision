#include "infTool/TwoCameraSpliceInfTool/TwoCameraSpliceInfTool.hpp"

#include <QTimer>

namespace infTool
{
	TwoCameraSpliceInfTool::TwoCameraSpliceInfTool(inf::infrastructure& inf, CalibInfTool& calibTool)
		: inf_(inf)
		, calib_tool_(calibTool)
	{
		// 超时定时器：单次触发，500ms 内未收到配对帧则直通
		stitchTimeout_ = new QTimer(this);
		stitchTimeout_->setSingleShot(true);
		QObject::connect(stitchTimeout_, &QTimer::timeout,
			this, &TwoCameraSpliceInfTool::onStitchTimeout);
	}

	void TwoCameraSpliceInfTool::calculateTwoCameraSpliceConfig(const HalconCpp::HImage& himage1,
		const HalconCpp::HImage& himage2)
	{
		// 复用已实现的 calibImage 计算拼接映射，结果写入拼接配置模块并持久化。
		Config::TwoCameraSpliceCfg result = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

		// 双相机标定参数：各相机独立标定数据。
		auto& calib = inf_.calib_config_module_->calibConfig;

		std::string err;
		const bool ok = calibImage(
			static_cast<HalconCpp::HObject>(himage1),
			static_cast<HalconCpp::HObject>(himage2),
			calib.item(global::CameraIndex::Camera1),
			calib.item(global::CameraIndex::Camera2),
			result, &err);

		if (ok)
		{
			inf_.two_camera_splice_module_->twoCameraSpliceConfig = result;
			inf_.two_camera_splice_module_->save();
		}
	}

	bool TwoCameraSpliceInfTool::calibImage(HalconCpp::HObject image1, HalconCpp::HObject image2,
		const Config::CalibConfigItem& cam1Calib, const Config::CalibConfigItem& cam2Calib,
		Config::TwoCameraSpliceCfg& spliceCfg,
		std::string* errorMsg)
	{
		// 检查输入图像
		if (!image1.IsInitialized() || !image2.IsInitialized())
		{
			if (errorMsg) *errorMsg = "相机图像为空";
			return false;
		}

		// 检查相机标定参数
		if (cam1Calib.cameraParameters.TupleLength() == 0 || cam1Calib.cameraPose.TupleLength() == 0 ||
		    cam2Calib.cameraParameters.TupleLength() == 0 || cam2Calib.cameraPose.TupleLength() == 0)
		{
			if (errorMsg) *errorMsg = "相机标定参数不完整";
			return false;
		}

		// 检查标定板描述文件路径
		if (spliceCfg.caltabDescrPath.empty())
		{
			if (errorMsg) *errorMsg = "标定板描述文件路径为空";
			return false;
		}

		// 局部变量声明
		HalconCpp::HTuple hv_CalibDataID;
		HalconCpp::HTuple hv_RowCoord1, hv_ColumnCoord1, hv_Index1, hv_Pose1;
		HalconCpp::HTuple hv_RowCoord2, hv_ColumnCoord2, hv_Index2, hv_Pose2;
		HalconCpp::HTuple hv_Width, hv_Height;
		HalconCpp::HTuple hv_WidthImage1, hv_HeightImage1;
		HalconCpp::HTuple hv_UpperRow, hv_LeftColumn, hv_LeftX, hv_UpperY;
		HalconCpp::HTuple hv_LowerRow, hv_X1, hv_LowerY, hv_HeightRect;
		HalconCpp::HTuple hv_RightColumn, hv_RightX, hv_Y1, hv_WidthRect;
		HalconCpp::HTuple hv_PoseNewOrigin1, hv_PoseNewOrigin2;
		HalconCpp::HTuple hv_HomMat3DIdentity, hv_cp1Hur1, hv_cp1Hcp2, hv_cp2Hcp1, hv_cp2Hul2;
		HalconCpp::HTuple hv_cam2Hcp2, hv_cam2Hul2;

		HalconCpp::HObject ho_Map1, ho_Map2;

		// 辅助 lambda：从 area_scan_polynomial 相机参数中提取各字段
		auto parseCamParams = [](const HalconCpp::HTuple& camParam,
			HalconCpp::HTuple& hv_Focus,
			HalconCpp::HTuple& hv_K1, HalconCpp::HTuple& hv_K2, HalconCpp::HTuple& hv_K3,
			HalconCpp::HTuple& hv_P1, HalconCpp::HTuple& hv_P2,
			HalconCpp::HTuple& hv_Sx, HalconCpp::HTuple& hv_Sy,
			HalconCpp::HTuple& hv_Cx, HalconCpp::HTuple& hv_Cy,
			HalconCpp::HTuple& hv_ImageWidth, HalconCpp::HTuple& hv_ImageHeight) -> bool
		{
			if (camParam.TupleLength() < 13)
				return false;
			hv_Focus = camParam[1];
			hv_K1 = camParam[2];
			hv_K2 = camParam[3];
			hv_K3 = camParam[4];
			hv_P1 = camParam[5];
			hv_P2 = camParam[6];
			hv_Sx = camParam[7];
			hv_Sy = camParam[8];
			hv_Cx = camParam[9];
			hv_Cy = camParam[10];
			hv_ImageWidth = camParam[11];
			hv_ImageHeight = camParam[12];
			return true;
		};

		// 辅助 lambda：生成 area_scan_polynomial 相机参数
		auto genCamParAreaScanPolynomial = [](HalconCpp::HTuple hv_Focus,
			HalconCpp::HTuple hv_K1, HalconCpp::HTuple hv_K2, HalconCpp::HTuple hv_K3,
			HalconCpp::HTuple hv_P1, HalconCpp::HTuple hv_P2,
			HalconCpp::HTuple hv_Sx, HalconCpp::HTuple hv_Sy, HalconCpp::HTuple hv_Cx, HalconCpp::HTuple hv_Cy,
			HalconCpp::HTuple hv_ImageWidth, HalconCpp::HTuple hv_ImageHeight,
			HalconCpp::HTuple* hv_CameraParam)
		{
			(*hv_CameraParam).Clear();
			(*hv_CameraParam)[0] = "area_scan_polynomial";
			(*hv_CameraParam).Append(hv_Focus);
			(*hv_CameraParam).Append(hv_K1);
			(*hv_CameraParam).Append(hv_K2);
			(*hv_CameraParam).Append(hv_K3);
			(*hv_CameraParam).Append(hv_P1);
			(*hv_CameraParam).Append(hv_P2);
			(*hv_CameraParam).Append(hv_Sx);
			(*hv_CameraParam).Append(hv_Sy);
			(*hv_CameraParam).Append(hv_Cx);
			(*hv_CameraParam).Append(hv_Cy);
			(*hv_CameraParam).Append(hv_ImageWidth);
			(*hv_CameraParam).Append(hv_ImageHeight);
		};

		HalconCpp::HTuple hv_Focus1, hv_K11, hv_K21, hv_K31, hv_P11, hv_P21, hv_Sx1, hv_Sy1, hv_Cx1, hv_Cy1, hv_ImageWidth1, hv_ImageHeight1;
		HalconCpp::HTuple hv_Focus2, hv_K12, hv_K22, hv_K32, hv_P12, hv_P22, hv_Sx2, hv_Sy2, hv_Cx2, hv_Cy2, hv_ImageWidth2, hv_ImageHeight2;

		if (!parseCamParams(cam1Calib.cameraParameters, hv_Focus1, hv_K11, hv_K21, hv_K31, hv_P11, hv_P21, hv_Sx1, hv_Sy1, hv_Cx1, hv_Cy1, hv_ImageWidth1, hv_ImageHeight1))
		{
			if (errorMsg) *errorMsg = "相机1参数格式不正确";
			return false;
		}
		if (!parseCamParams(cam2Calib.cameraParameters, hv_Focus2, hv_K12, hv_K22, hv_K32, hv_P12, hv_P22, hv_Sx2, hv_Sy2, hv_Cx2, hv_Cy2, hv_ImageWidth2, hv_ImageHeight2))
		{
			if (errorMsg) *errorMsg = "相机2参数格式不正确";
			return false;
		}

		try
		{
			HalconCpp::HTuple hv_CamParam1, hv_CamParam2;
			genCamParAreaScanPolynomial(hv_Focus1, hv_K11, hv_K21, hv_K31, hv_P11, hv_P21, hv_Sx1, hv_Sy1, hv_Cx1, hv_Cy1, hv_ImageWidth1, hv_ImageHeight1, &hv_CamParam1);
			genCamParAreaScanPolynomial(hv_Focus2, hv_K12, hv_K22, hv_K32, hv_P12, hv_P22, hv_Sx2, hv_Sy2, hv_Cx2, hv_Cy2, hv_ImageWidth2, hv_ImageHeight2, &hv_CamParam2);

			hv_Width = hv_ImageWidth1;
			hv_Height = hv_ImageHeight1;

			HalconCpp::CreateCalibData("calibration_object", 2, 1, &hv_CalibDataID);
			HalconCpp::SetCalibDataCalibObject(hv_CalibDataID, 0, HalconCpp::HTuple(spliceCfg.caltabDescrPath.c_str()));
			HalconCpp::SetCalibDataCamParam(hv_CalibDataID, 0, HalconCpp::HTuple(), hv_CamParam1);
			HalconCpp::SetCalibDataCamParam(hv_CalibDataID, 1, HalconCpp::HTuple(), hv_CamParam2);

			try
			{
				HalconCpp::FindCalibObject(image1, hv_CalibDataID, 0, 0, 0, HalconCpp::HTuple(), HalconCpp::HTuple());
				HalconCpp::GetCalibDataObservPoints(hv_CalibDataID, 0, 0, 0,
					&hv_RowCoord1, &hv_ColumnCoord1, &hv_Index1, &hv_Pose1);
			}
			catch (HalconCpp::HException& except)
			{
				HalconCpp::ClearCalibData(hv_CalibDataID);
				if (errorMsg) *errorMsg = "相机1图像中未找到标定板";
				return false;
			}

			try
			{
				HalconCpp::FindCalibObject(image2, hv_CalibDataID, 1, 0, 0, HalconCpp::HTuple(), HalconCpp::HTuple());
				HalconCpp::GetCalibDataObservPoints(hv_CalibDataID, 1, 0, 0,
					&hv_RowCoord2, &hv_ColumnCoord2, &hv_Index2, &hv_Pose2);
			}
			catch (HalconCpp::HException& except)
			{
				HalconCpp::ClearCalibData(hv_CalibDataID);
				if (errorMsg) *errorMsg = "相机2图像中未找到标定板";
				return false;
			}

			HalconCpp::ClearCalibData(hv_CalibDataID);

			HalconCpp::GetImageSize(image1, &hv_WidthImage1, &hv_HeightImage1);

			hv_UpperRow = (hv_HeightImage1 * spliceCfg.BorderInPercent) / 100.0;
			hv_LeftColumn = (hv_WidthImage1 * spliceCfg.BorderInPercent) / 100.0;

			HalconCpp::ImagePointsToWorldPlane(hv_CamParam1, hv_Pose1, hv_UpperRow, hv_LeftColumn, "m",
				&hv_LeftX, &hv_UpperY);

			hv_LowerRow = (hv_HeightImage1 * (100.0 - spliceCfg.BorderInPercent)) / 100.0;
			HalconCpp::ImagePointsToWorldPlane(hv_CamParam1, hv_Pose1, hv_LowerRow, hv_LeftColumn, "m",
				&hv_X1, &hv_LowerY);

			hv_HeightRect = ((hv_LowerY - hv_UpperY) / spliceCfg.pixTowWorld).TupleInt();

			hv_RightColumn = (hv_WidthImage1 * (100.0 - (spliceCfg.OverlapInPercent / 2.0))) / 100.0;
			HalconCpp::ImagePointsToWorldPlane(hv_CamParam1, hv_Pose1, hv_UpperRow, hv_RightColumn, "m",
				&hv_RightX, &hv_Y1);

			hv_WidthRect = ((hv_RightX - hv_LeftX) / spliceCfg.pixTowWorld).TupleInt();

			spliceCfg.rectifiedWidth = hv_WidthRect.I();
			spliceCfg.rectifiedHeight = hv_HeightRect.I();

			HalconCpp::SetOriginPose(hv_Pose1, hv_LeftX, hv_UpperY, spliceCfg.DiffHeight, &hv_PoseNewOrigin1);
			HalconCpp::GenImageToWorldPlaneMap(&ho_Map1, hv_CamParam1, hv_PoseNewOrigin1,
				hv_Width, hv_Height, hv_WidthRect, hv_HeightRect, spliceCfg.pixTowWorld, "bilinear");

			HalconCpp::HomMat3dIdentity(&hv_HomMat3DIdentity);

			HalconCpp::HomMat3dTranslateLocal(hv_HomMat3DIdentity,
				hv_LeftX + (spliceCfg.pixTowWorld * hv_WidthRect), hv_UpperY, spliceCfg.DiffHeight, &hv_cp1Hur1);

			HalconCpp::HomMat3dTranslateLocal(hv_HomMat3DIdentity, spliceCfg.DistancePlates, 0, 0, &hv_cp1Hcp2);

			HalconCpp::HomMat3dInvert(hv_cp1Hcp2, &hv_cp2Hcp1);
			HalconCpp::HomMat3dCompose(hv_cp2Hcp1, hv_cp1Hur1, &hv_cp2Hul2);

			HalconCpp::PoseToHomMat3d(hv_Pose2, &hv_cam2Hcp2);
			HalconCpp::HomMat3dCompose(hv_cam2Hcp2, hv_cp2Hul2, &hv_cam2Hul2);
			HalconCpp::HomMat3dToPose(hv_cam2Hul2, &hv_PoseNewOrigin2);

			HalconCpp::GenImageToWorldPlaneMap(&ho_Map2, hv_CamParam2, hv_PoseNewOrigin2,
				hv_Width, hv_Height, hv_WidthRect, hv_HeightRect, spliceCfg.pixTowWorld, "bilinear");

			spliceCfg.MapSingle1 = ho_Map1;
			spliceCfg.MapSingle2 = ho_Map2;

			return true;
		}
		catch (HalconCpp::HException& except)
		{
			if (hv_CalibDataID.TupleLength() > 0)
			{
				try { HalconCpp::ClearCalibData(hv_CalibDataID); } catch (...) {}
			}
			if (errorMsg) *errorMsg = std::string("标定过程异常: ") + except.ErrorMessage().Text();
			return false;
		}
	}

	bool TwoCameraSpliceInfTool::pinjieImage(HalconCpp::HObject& image1, HalconCpp::HObject& image2,
		Config::TwoCameraSpliceCfg& spliceCfg,
		HalconCpp::HObject& stitchedImage)
	{
		if (!spliceCfg.MapSingle1.IsInitialized() || !spliceCfg.MapSingle2.IsInitialized())
			return false;

		try
		{
			HalconCpp::HObject ho_RectifiedImage1, ho_RectifiedImage2, ho_Concat;

			HalconCpp::MapImage(image1, spliceCfg.MapSingle1, &ho_RectifiedImage1);
			HalconCpp::MapImage(image2, spliceCfg.MapSingle2, &ho_RectifiedImage2);

			HalconCpp::ConcatObj(ho_RectifiedImage1, ho_RectifiedImage2, &ho_Concat);
			HalconCpp::TileImages(ho_Concat, &stitchedImage, 2, "horizontal");

			return stitchedImage.IsInitialized();
		}
		catch (HalconCpp::HException&)
		{
			return false;
		}
	}

	void TwoCameraSpliceInfTool::onCalibFrame(HalconCpp::HImage img, global::CameraIndex cameraIndex)
	{
		// 1. 缓存当前相机帧（覆盖旧帧，保证始终是最新帧）
		switch (cameraIndex)
		{
		case global::CameraIndex::Camera1:
			cam1_image_ = img;
			cam1_ready_ = true;
			break;
		case global::CameraIndex::Camera2:
			cam2_image_ = img;
			cam2_ready_ = true;
			break;
		}

		// 2. 检查另一相机是否物理在线
		const auto otherIdx = (cameraIndex == global::CameraIndex::Camera1)
			? global::CameraIndex::Camera2 : global::CameraIndex::Camera1;
		const bool otherOnline = inf_.camera_module_
			&& inf_.camera_module_->isConnected(otherIdx);

		// 3. 另一相机离线 → 直通当前帧（不等、不拼）
		if (!otherOnline)
		{
			stitchTimeout_->stop();
			cam1_ready_ = false;
			cam2_ready_ = false;
			emit callBackFunc(img);
			return;
		}

		// 4. 双路均在线，且帧均已就绪 → 拼接
		if (cam1_ready_ && cam2_ready_)
		{
			stitchTimeout_->stop();
			cam1_ready_ = false;
			cam2_ready_ = false;
			tryStitchAndEmit();
			return;
		}

		// 5. 等待配对帧：每收到新帧都重置超时，保证从最新帧算起等足 500ms
		stitchTimeout_->start(500);
	}

	void TwoCameraSpliceInfTool::onStitchTimeout()
	{
		// 超时：另一相机在线但帧未到达 → 直通已缓存单路帧
		if (cam1_ready_)
		{
			cam1_ready_ = false;
			emit callBackFunc(cam1_image_);
		}
		else if (cam2_ready_)
		{
			cam2_ready_ = false;
			emit callBackFunc(cam2_image_);
		}
	}

	void TwoCameraSpliceInfTool::tryStitchAndEmit()
	{
		auto& spliceCfg = inf_.two_camera_splice_module_->twoCameraSpliceConfig;
		const bool hasSpliceConfig = spliceCfg.MapSingle1.IsInitialized()
			&& spliceCfg.MapSingle2.IsInitialized();

		HalconCpp::HObject outImage;
		bool ok = false;

		// 优先几何拼接
		if (hasSpliceConfig)
		{
			try
			{
				HalconCpp::HObject ho1 = static_cast<HalconCpp::HObject>(cam1_image_);
				HalconCpp::HObject ho2 = static_cast<HalconCpp::HObject>(cam2_image_);
				ok = pinjieImage(ho1, ho2, spliceCfg, outImage);
			}
			catch (const HalconCpp::HException&) { ok = false; }
		}

		// 降级：硬拼接 — 两张原图左右平铺
		if (!ok)
		{
			try
			{
				HalconCpp::HObject hoConcat;
				HalconCpp::ConcatObj(cam1_image_, cam2_image_, &hoConcat);
				HalconCpp::TileImages(hoConcat, &outImage, 2, "horizontal");
				ok = outImage.IsInitialized();
			}
			catch (const HalconCpp::HException&) { ok = false; }
		}

		if (ok)
			emit callBackFunc(HalconCpp::HImage(outImage));
	}

	void TwoCameraSpliceInfTool::build()
	{
		// 接收 CalibInfTool 矫正后的双相机图像，驱动拼接流程
		connect(&calib_tool_, &CalibInfTool::callBackFunc,
			this, &TwoCameraSpliceInfTool::onCalibFrame, Qt::DirectConnection);
	}

	void TwoCameraSpliceInfTool::destroy()
	{
		stitchTimeout_->stop();
		disconnect(&calib_tool_, &CalibInfTool::callBackFunc,
			this, &TwoCameraSpliceInfTool::onCalibFrame);
	}

	void TwoCameraSpliceInfTool::start()
	{
	}

	void TwoCameraSpliceInfTool::stop()
	{
		stitchTimeout_->stop();
		cam1_ready_ = false;
		cam2_ready_ = false;
	}
}
