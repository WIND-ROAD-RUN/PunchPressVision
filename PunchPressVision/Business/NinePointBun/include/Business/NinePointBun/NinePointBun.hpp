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

		//TODO:这里定义九点标定的算法，包含输入类型的定义和返回结果的定义，下面是一个模板
		void calculateNinePointConfig();

	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
