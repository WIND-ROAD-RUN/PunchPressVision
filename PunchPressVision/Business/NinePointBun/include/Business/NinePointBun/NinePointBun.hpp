#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

namespace bun
{
	class NinePointBun
		: public global::IBusiness
	{
	public:
		explicit NinePointBun(inf::infrastructure& inf);
	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
