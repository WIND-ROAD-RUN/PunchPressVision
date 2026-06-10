#pragma once

#include <QObject>

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

namespace bun
{
	// 光源控制业务（FR-021、FR-022）：上/下光源开关，
	// 训练模型时由上层读取当前光源状态写入模型元数据（FR-025）。
	class LightControlBun
		: public QObject, public global::IBusiness
	{
		Q_OBJECT
	public:
		explicit LightControlBun(inf::infrastructure& inf);

		void setUpperLight(bool on);
		void setLowerLight(bool on);
		bool getUpperLightState() const;
		bool getLowerLightState() const;

		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;

	signals:
		void upperLightChanged(bool on);
		void lowerLightChanged(bool on);

	private:
		inf::infrastructure& inf_;
	};
}
