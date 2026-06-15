#pragma once

#include "infTool/infTool.hpp"
#include "Business/CalibBun/CalibBun.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "Business/LightControlBun/LightControlBun.hpp"

namespace bun
{
	// 保持旧命名空间别名，减少 App/UI 改动
	using CameraBun = infTool::CameraBun;
	using NinePointBun = infTool::NinePointBun;
	using TwoCameraSpliceBun = infTool::TwoCameraSpliceBun;
	using Point2D = infTool::Point2D;
	using PointPair = infTool::PointPair;

	class Business
	{
	public:
		explicit Business(inf::infrastructure& inf);
		~Business()=default;
	private:
		inf::infrastructure& inf_;
		std::unique_ptr<infTool::infTool> infTool_;
	public:
		void build();
		void destroy();
		void start();
		void stop();

		// 供 App 层查询基础设施状态（只读访问配置就绪性等）
		inf::infrastructure& infrastructure() { return inf_; }
		const inf::infrastructure& infrastructure() const { return inf_; }

		// 对 infTool 聚合的访问
		infTool::infTool& infTool() { return *infTool_; }
		const infTool::infTool& infTool() const { return *infTool_; }
	public:
		infTool::CameraBun* camera_bun{ nullptr };
		std::unique_ptr<CalibBun> calib_bun;
		infTool::NinePointBun* nine_point_bun{ nullptr };
		std::unique_ptr<ShapeModeManagerBun> shape_mode_manager_bun;
		infTool::TwoCameraSpliceBun* two_camera_splice_bun{ nullptr };
		std::unique_ptr<LightControlBun> light_control_bun;
	};
}
