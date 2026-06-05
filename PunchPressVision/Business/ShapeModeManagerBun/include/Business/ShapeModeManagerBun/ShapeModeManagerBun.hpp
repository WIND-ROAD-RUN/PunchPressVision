#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

namespace bun
{
	class ShapeModeManagerBun
		: public global::IBusiness
	{
	public:
		explicit ShapeModeManagerBun(inf::infrastructure& inf);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
