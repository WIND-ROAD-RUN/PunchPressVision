#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

namespace bun
{
	class CalibBun
		: public global::IBusiness
	{
	public:
		explicit CalibBun(inf::infrastructure& inf);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
