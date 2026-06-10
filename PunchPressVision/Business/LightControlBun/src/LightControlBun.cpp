#include "Business/LightControlBun/LightControlBun.hpp"

namespace bun
{
	LightControlBun::LightControlBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void LightControlBun::setUpperLight(bool on)
	{
		if (inf_.light_io_module_)
			inf_.light_io_module_->setUpperLight(on);
		emit upperLightChanged(on);
	}

	void LightControlBun::setLowerLight(bool on)
	{
		if (inf_.light_io_module_)
			inf_.light_io_module_->setLowerLight(on);
		emit lowerLightChanged(on);
	}

	bool LightControlBun::getUpperLightState() const
	{
		return inf_.light_io_module_ ? inf_.light_io_module_->getUpperLightState() : false;
	}

	bool LightControlBun::getLowerLightState() const
	{
		return inf_.light_io_module_ ? inf_.light_io_module_->getLowerLightState() : false;
	}

	void LightControlBun::build()
	{
	}

	void LightControlBun::destroy()
	{
	}

	void LightControlBun::start()
	{
	}

	void LightControlBun::stop()
	{
	}
}
