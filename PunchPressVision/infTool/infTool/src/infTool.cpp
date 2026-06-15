#include "infTool/infTool.hpp"

namespace infTool
{
	infTool::infTool(inf::infrastructure& inf)
		: inf_(inf)
	{
		calib_bun = std::make_unique<CalibInfTool>(inf_);
		nine_point_bun = std::make_unique<NinePointInfTool>(inf_);
		two_camera_splice_bun = std::make_unique<TwoCameraSpliceInfTool>(inf_, *calib_bun);
	}

	infTool::~infTool()
	{
		// 构造逆序显式 reset，确保依赖关系正确析构
		calib_bun.reset();
		nine_point_bun.reset();
		two_camera_splice_bun.reset();
	}

	void infTool::build()
	{
		if (two_camera_splice_bun) two_camera_splice_bun->build();
		if (nine_point_bun) nine_point_bun->build();
		if (calib_bun) calib_bun->build();
	}

	void infTool::destroy()
	{
		if (calib_bun) calib_bun->destroy();
		if (nine_point_bun) nine_point_bun->destroy();
		if (two_camera_splice_bun) two_camera_splice_bun->destroy();
	}

	void infTool::start()
	{
		if (two_camera_splice_bun) two_camera_splice_bun->start();
		if (nine_point_bun) nine_point_bun->start();
		if (calib_bun) calib_bun->start();
	}

	void infTool::stop()
	{
		if (calib_bun) calib_bun->stop();
		if (nine_point_bun) nine_point_bun->stop();
		if (two_camera_splice_bun) two_camera_splice_bun->stop();
	}
}
