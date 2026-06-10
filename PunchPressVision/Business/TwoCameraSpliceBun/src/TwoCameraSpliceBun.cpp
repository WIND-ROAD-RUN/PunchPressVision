#include "Business/TwoCameraSpliceBun/TwoCameraSpliceBun.hpp"

namespace bun
{
	TwoCameraSpliceBun::TwoCameraSpliceBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void TwoCameraSpliceBun::calculateTwoCameraSpliceConfig(const HalconCpp::HImage& himage1,
		const HalconCpp::HImage& himage2)
	{
		// 复用已实现的 calibImage 计算拼接映射，结果写入拼接配置模块并持久化。
		Config::TwoCameraSpliceCfg result = inf_.two_camera_splice_module_->twoCameraSpliceConfig;

		// 双相机标定参数：当前以同一相机标定配置为基准（单相机标定数据）。
		// TODO: 若两相机各自独立标定，应分别传入对应的 CalibConfig。
		Config::CalibConfig calib = inf_.calib_config_module_->calibConfig;

		std::string err;
		const bool ok = calibImage(
			static_cast<HalconCpp::HObject>(himage1),
			static_cast<HalconCpp::HObject>(himage2),
			calib, calib, result, &err);

		if (ok)
		{
			inf_.two_camera_splice_module_->twoCameraSpliceConfig = result;
			inf_.two_camera_splice_module_->save();
		}
	}

	bool TwoCameraSpliceBun::calibImage(HalconCpp::HObject image1, HalconCpp::HObject image2,
		Config::CalibConfig cam1Calib, Config::CalibConfig cam2Calib,
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
			// [0]=camera_type, [1]=focus(m), [2]=k1, [3]=k2, [4]=k3, [5]=p1, [6]=p2,
			// [7]=sx(m/px), [8]=sy(m/px), [9]=cx(px), [10]=cy(px), [11]=width(px), [12]=height(px)
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
			// 生成相机参数（如有需要可在此修改参数值）
			HalconCpp::HTuple hv_CamParam1, hv_CamParam2;
			genCamParAreaScanPolynomial(hv_Focus1, hv_K11, hv_K21, hv_K31, hv_P11, hv_P21, hv_Sx1, hv_Sy1, hv_Cx1, hv_Cy1, hv_ImageWidth1, hv_ImageHeight1, &hv_CamParam1);
			genCamParAreaScanPolynomial(hv_Focus2, hv_K12, hv_K22, hv_K32, hv_P12, hv_P22, hv_Sx2, hv_Sy2, hv_Cx2, hv_Cy2, hv_ImageWidth2, hv_ImageHeight2, &hv_CamParam2);

			// 使用解析出的图像尺寸
			hv_Width = hv_ImageWidth1;
			hv_Height = hv_ImageHeight1;

			//========== 1. 准备相机标定数据 ==========
			HalconCpp::CreateCalibData("calibration_object", 2, 1, &hv_CalibDataID);
			HalconCpp::SetCalibDataCalibObject(hv_CalibDataID, 0, HalconCpp::HTuple(spliceCfg.caltabDescrPath.c_str()));
			HalconCpp::SetCalibDataCamParam(hv_CalibDataID, 0, HalconCpp::HTuple(), hv_CamParam1);
			HalconCpp::SetCalibDataCamParam(hv_CalibDataID, 1, HalconCpp::HTuple(), hv_CamParam2);

			//========== 2. 在图像1中查找标定板 ==========
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

			//========== 3. 在图像2中查找标定板 ==========
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

			//========== 4. 清理标定数据 ==========
			HalconCpp::ClearCalibData(hv_CalibDataID);

			//========== 5. 计算矫正图像的尺寸 ==========
			HalconCpp::GetImageSize(image1, &hv_WidthImage1, &hv_HeightImage1);

			// 计算左上角的图像坐标
			hv_UpperRow = (hv_HeightImage1 * spliceCfg.BorderInPercent) / 100.0;
			hv_LeftColumn = (hv_WidthImage1 * spliceCfg.BorderInPercent) / 100.0;

			// 将左上角点转换到世界坐标系
			HalconCpp::ImagePointsToWorldPlane(hv_CamParam1, hv_Pose1, hv_UpperRow, hv_LeftColumn, "m",
				&hv_LeftX, &hv_UpperY);

			// 计算左下角的图像坐标并转换到世界坐标系
			hv_LowerRow = (hv_HeightImage1 * (100.0 - spliceCfg.BorderInPercent)) / 100.0;
			HalconCpp::ImagePointsToWorldPlane(hv_CamParam1, hv_Pose1, hv_LowerRow, hv_LeftColumn, "m",
				&hv_X1, &hv_LowerY);

			// 计算矫正后的图像高度（像素）
			hv_HeightRect = ((hv_LowerY - hv_UpperY) / spliceCfg.pixTowWorld).TupleInt();

			// 计算右侧列坐标（考虑重叠区域）
			hv_RightColumn = (hv_WidthImage1 * (100.0 - (spliceCfg.OverlapInPercent / 2.0))) / 100.0;
			HalconCpp::ImagePointsToWorldPlane(hv_CamParam1, hv_Pose1, hv_UpperRow, hv_RightColumn, "m",
				&hv_RightX, &hv_Y1);

			// 计算矫正后的图像宽度（像素）
			hv_WidthRect = ((hv_RightX - hv_LeftX) / spliceCfg.pixTowWorld).TupleInt();

			// 保存矫正后的图像尺寸
			spliceCfg.rectifiedWidth = hv_WidthRect.I();
			spliceCfg.rectifiedHeight = hv_HeightRect.I();

			//========== 6. 生成相机1的映射图 ==========
			HalconCpp::SetOriginPose(hv_Pose1, hv_LeftX, hv_UpperY, spliceCfg.DiffHeight, &hv_PoseNewOrigin1);
			HalconCpp::GenImageToWorldPlaneMap(&ho_Map1, hv_CamParam1, hv_PoseNewOrigin1,
				hv_Width, hv_Height, hv_WidthRect, hv_HeightRect, spliceCfg.pixTowWorld, "bilinear");

			//========== 7. 生成相机2的映射图 ==========
			// 创建单位矩阵
			HalconCpp::HomMat3dIdentity(&hv_HomMat3DIdentity);

			// 计算从第一个标定板坐标系到右上角点的变换
			HalconCpp::HomMat3dTranslateLocal(hv_HomMat3DIdentity,
				hv_LeftX + (spliceCfg.pixTowWorld * hv_WidthRect), hv_UpperY, spliceCfg.DiffHeight, &hv_cp1Hur1);

			// 计算两个标定板之间的变换
			HalconCpp::HomMat3dTranslateLocal(hv_HomMat3DIdentity, spliceCfg.DistancePlates, 0, 0, &hv_cp1Hcp2);

			// 计算第二个图像的位姿变换
			HalconCpp::HomMat3dInvert(hv_cp1Hcp2, &hv_cp2Hcp1);
			HalconCpp::HomMat3dCompose(hv_cp2Hcp1, hv_cp1Hur1, &hv_cp2Hul2);

			// 计算相机2的新位姿
			HalconCpp::PoseToHomMat3d(hv_Pose2, &hv_cam2Hcp2);
			HalconCpp::HomMat3dCompose(hv_cam2Hcp2, hv_cp2Hul2, &hv_cam2Hul2);
			HalconCpp::HomMat3dToPose(hv_cam2Hul2, &hv_PoseNewOrigin2);

			// 生成相机2的映射图
			HalconCpp::GenImageToWorldPlaneMap(&ho_Map2, hv_CamParam2, hv_PoseNewOrigin2,
				hv_Width, hv_Height, hv_WidthRect, hv_HeightRect, spliceCfg.pixTowWorld, "bilinear");

			//========== 8. 设置输出 ==========
			spliceCfg.MapSingle1 = ho_Map1;
			spliceCfg.MapSingle2 = ho_Map2;

			return true;
		}
		catch (HalconCpp::HException& except)
		{
			// 异常处理：清理资源
			if (hv_CalibDataID.TupleLength() > 0)
			{
				try { HalconCpp::ClearCalibData(hv_CalibDataID); } catch (...) {}
			}
			if (errorMsg) *errorMsg = std::string("标定过程异常: ") + except.ErrorMessage().Text();
			return false;
		}
	}

	bool TwoCameraSpliceBun::pinjieImage(HalconCpp::HObject& image1, HalconCpp::HObject& image2,
		Config::TwoCameraSpliceCfg& spliceCfg,
		HalconCpp::HObject& stitchedImage)
	{
		// 检查映射图是否已经生成
		if (!spliceCfg.MapSingle1.IsInitialized() || !spliceCfg.MapSingle2.IsInitialized())
		{
			return false;
		}

		try
		{
			HalconCpp::HObject ho_RectifiedImage1, ho_RectifiedImage2, ho_Concat;

			//========== 1. 使用映射图矫正两张图片 ==========
			HalconCpp::MapImage(image1, spliceCfg.MapSingle1, &ho_RectifiedImage1);
			HalconCpp::MapImage(image2, spliceCfg.MapSingle2, &ho_RectifiedImage2);

			//========== 2. 将两张矫正后的图片合并到对象数组 ==========
			HalconCpp::ConcatObj(ho_RectifiedImage1, ho_RectifiedImage2, &ho_Concat);

			//========== 3. 将两张图片平铺成一张大图（2列，水平排列） ==========
			HalconCpp::TileImages(ho_Concat, &stitchedImage, 2, "horizontal");

			return stitchedImage.IsInitialized();
		}
		catch (HalconCpp::HException& except)
		{
			return false;
		}
	}

	void TwoCameraSpliceBun::build()
	{
	}

	void TwoCameraSpliceBun::destroy()
	{
	}

	void TwoCameraSpliceBun::start()
	{
	}

	void TwoCameraSpliceBun::stop()
	{
	}
}
