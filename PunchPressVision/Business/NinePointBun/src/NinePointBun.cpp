#include "Business/NinePointBun/NinePointBun.hpp"

namespace bun
{
	NinePointBun::NinePointBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void NinePointBun::calculateNinePointConfig()
	{
		Config::NinePointCfg ninePointCfg;

		//TODO:

		inf_.nine_point_module_->ninePointConfig = ninePointCfg;
	}

	void NinePointBun::build()
	{
	}

	void NinePointBun::destroy()
	{
	}

	void NinePointBun::start()
	{
	}

	void NinePointBun::stop()
	{
	}
}
