#pragma once

#include "infrastructure/infrastructure.hpp"
#include "Business/CalibBun/CalibBun.hpp"
#include "Business/CameraBun/CameraBun.hpp"
#include "Business/NinePointBun/NinePointBun.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "Business/TwoCameraSpliceBun/TwoCameraSpliceBun.hpp"

namespace bun
{
	class Business
	{
	public:
		explicit Business(inf::infrastructure& inf);
		~Business()=default;
	private:
		inf::infrastructure& inf_;
	public:
		void build();
		void destroy();
		void start();
		void stop();
	public:
		std::unique_ptr<CameraBun> camera_bun;
		std::unique_ptr<CalibBun> calib_bun;
		std::unique_ptr<NinePointBun> nine_point_bun;
		std::unique_ptr<ShapeModeManagerBun> shape_mode_manager_bun;
		std::unique_ptr<TwoCameraSpliceBun> two_camera_splice_bun;
	};
}
