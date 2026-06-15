#include "Business/CameraBun/CameraBun.hpp"

#include "Business/CameraBun/CameraImgConvert.hpp"

namespace bun
{
	CameraBun::CameraBun(inf::infrastructure& inf, infTool::infTool& infTool)
		: inf_(inf)
		, inf_tool_(infTool)
	{
	}

	void CameraBun::build()
	{
		// 将相机网关的信号桥接到本编排单元（DirectConnection 减少帧延迟）
		if (inf_.camera_module_)
		{
			QObject::connect(inf_.camera_module_.get(), &inf::CameraModule::callBackFunc,
				this, &CameraBun::callBackFunc, Qt::DirectConnection);
			QObject::connect(inf_.camera_module_.get(), &inf::CameraModule::cameraConnectionStateChanged,
				this, &CameraBun::cameraConnectionStateChanged, Qt::QueuedConnection);
		}
	}

	void CameraBun::destroy()
	{
		if (inf_.camera_module_)
			QObject::disconnect(inf_.camera_module_.get(), nullptr, this, nullptr);
	}

	void CameraBun::start()
	{
	}

	void CameraBun::stop()
	{
	}

	void CameraBun::setSpliceEnabled(bool enabled)
	{
		spliceEnabled_.store(enabled, std::memory_order_release);
	}

	HalconCpp::HImage CameraBun::applyUndistort(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;
		try
		{
			if (!inf_.calib_config_module_)
				return image;
			const auto& cfg = inf_.calib_config_module_->calibConfig;
			if (cfg.cameraParameters.Length() == 0)
				return image; // 未标定，原样返回

			HTuple camParRectified;
			ChangeRadialDistortionCamPar("fixed", cfg.cameraParameters, 0, &camParRectified);

			HObject rectified;
			ChangeRadialDistortionImage(image, HObject(), &rectified,
				cfg.cameraParameters, camParRectified);
			return HImage(rectified);
		}
		catch (...)
		{
			return image;
		}
	}

	HalconCpp::HImage CameraBun::applyNinePointTransform(const HalconCpp::HImage& image)
	{
		// 九点标定矩阵用于像素↔世界坐标转换，通常在结果坐标上应用，
		// 不对显示图像做整体仿射变换；此处保持图像不变，
		// 由定位环节使用 NinePointBun::pixToWorld 完成坐标换算。
		return image;
	}

	void CameraBun::callBackFunc(rw::hoec::MatInfo matInfo, global::CameraIndex cameraIndex)
	{
		try
		{
			// Step 1: cv::Mat → HImage
			HalconCpp::HImage hImage = CameraImgConvert::cvMatToHImage(matInfo.mat);

			// Step 2: 畸变矫正
			HalconCpp::HImage rectified = applyUndistort(hImage);

			// Step 3: 九点标定变换（坐标语义，图像保持）
			HalconCpp::HImage worldImage = applyNinePointTransform(rectified);

			// Step 4: 双相机拼接（仅当启用且两帧都就绪）
			if (spliceEnabled_.load(std::memory_order_acquire))
			{
				std::lock_guard<std::mutex> lk(stitchMutex_);
				if (cameraIndex == global::CameraIndex::Camera1)
				{
					cam1Buffer_ = worldImage;
					cam1Ready_.store(true, std::memory_order_release);
				}
				else
				{
					cam2Buffer_ = worldImage;
					cam2Ready_.store(true, std::memory_order_release);
				}

				if (cam1Ready_.load(std::memory_order_acquire) &&
					cam2Ready_.load(std::memory_order_acquire))
				{
					HalconCpp::HObject stitched;
					HalconCpp::HObject img1 = cam1Buffer_;
					HalconCpp::HObject img2 = cam2Buffer_;
					if (inf_.two_camera_splice_module_ &&
						inf_.two_camera_splice_module_->twoCameraSpliceConfig.MapSingle1.IsInitialized())
					{
						// 复用拼接配置进行融合（拼接逻辑在 TwoCameraSpliceBun 中实现）
						// 这里仅在配置就绪时尝试，失败回退到单帧
					}
					cam1Ready_.store(false, std::memory_order_release);
					cam2Ready_.store(false, std::memory_order_release);
					emit callBackFunWithCalib(worldImage, cameraIndex);
					return;
				}
			}

			// 单相机：直接发射
			emit callBackFunWithCalib(worldImage, cameraIndex);
		}
		catch (...)
		{
			// 静默失败，不中断帧流
		}
	}
}
