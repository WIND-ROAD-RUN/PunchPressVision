#include "Business/CameraBun/CameraBun.hpp"

#include "Business/CameraBun/CameraImgConvert.hpp"

namespace bun
{
	CameraBun::CameraBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void CameraBun::build()
	{
	}

	void CameraBun::destroy()
	{
	}

	void CameraBun::start()
	{
	}

	void CameraBun::stop()
	{
	}

	void CameraBun::callBackFunc(rw::rqwc::MatInfo matInfo, global::CameraIndex cameraIndex)
	{
		HalconCpp::HImage hImage = CameraImgConvert::cvMatToHImage(matInfo.mat);
		emit callBackFunWithCalib(hImage, cameraIndex);
	}
}
