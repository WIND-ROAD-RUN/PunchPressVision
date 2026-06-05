#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

namespace bun
{
	class CameraBun
		: public global::IBusiness
	{
	public:
		explicit CameraBun(inf::infrastructure& inf);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
