#pragma once

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"
#include <vector>

namespace bun
{
	struct Point2D
	{
		double x{ 0 };
		double y{ 0 };
	};

	struct PointPair
	{
		Point2D pixPosition;
		Point2D realPosition;
	};

	class NinePointBun
		: public global::IBusiness
	{
	public:
		explicit NinePointBun(inf::infrastructure& inf);

		//TODO:这里定义九点标定的算法，包含输入类型的定义和返回结果的定义，下面是一个模板
		void calculateNinePointConfig();

		bool calcPixToWorldHomMat2D(const std::vector<Point2D>& pixPoints,
			const std::vector<Point2D>& worldPoints,
			Config::NinePointCfg& ninePointCfg);
		//调用这个函数把像素值转化为实际值
		bool pixToWorld(const HalconCpp::HTuple& homMat2D,
			double pixX, double pixY,
			double& outWorldX, double& outWorldY);

	private:
		inf::infrastructure& inf_;
	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
