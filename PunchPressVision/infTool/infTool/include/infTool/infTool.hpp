#pragma once

#include <memory>

#include "infTool/CalibInfTool/CalibInfTool.hpp"
#include "infTool/NinePointInfTool/NinePointInfTool.hpp"
#include "infTool/TwoCameraSpliceInfTool/TwoCameraSpliceInfTool.hpp"
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
		std::unique_ptr<CalibInfTool> calib_bun;
		std::unique_ptr<NinePointInfTool> nine_point_bun;
		std::unique_ptr<TwoCameraSpliceInfTool> two_camera_splice_bun;

	private:
		inf::infrastructure& inf_;
	};
}
