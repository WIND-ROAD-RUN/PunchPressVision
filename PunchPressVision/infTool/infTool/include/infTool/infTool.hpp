#pragma once

#include <memory>

#include "infTool/CalibBun/CalibBun.hpp"
#include "infTool/NinePointBun/NinePointBun.hpp"
#include "infTool/TwoCameraSpliceBun/TwoCameraSpliceBun.hpp"
#include "infrastructure/infrastructure.hpp"

namespace infTool
{
	class infTool
	{
	public:
		explicit infTool(inf::infrastructure& inf);
		~infTool();

		void build();
		void destroy();
		void start();
		void stop();

	public:
		std::unique_ptr<CalibBun> calib_bun;
		std::unique_ptr<NinePointBun> nine_point_bun;
		std::unique_ptr<TwoCameraSpliceBun> two_camera_splice_bun;

	private:
		inf::infrastructure& inf_;
	};
}
