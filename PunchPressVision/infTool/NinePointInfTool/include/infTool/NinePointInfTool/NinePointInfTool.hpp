#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include <QObject>
#include <QString>

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"

namespace infTool
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

	class NinePointInfTool
		: public QObject, public global::IBusiness
	{
		Q_OBJECT
	public:
		explicit NinePointInfTool(inf::infrastructure& inf);
		~NinePointInfTool() override;

		//TODO:这里定义九点标定的算法，包含输入类型的定义和返回结果的定义
		void calculateNinePointConfig();

		bool calcPixToWorldHomMat2D(const std::vector<Point2D>& pixPoints,
			const std::vector<Point2D>& worldPoints,
			Config::NinePointCfg& ninePointCfg);
		//调用这个函数把像素值转化为实际值
		bool pixToWorld(const HalconCpp::HTuple& homMat2D,
			double pixX, double pixY,
			double& outWorldX, double& outWorldY);

		// 设置九点机械坐标（来自配置/UI）
		void setMechanicalCoords(const std::vector<Point2D>& coords);

		// 自动化九点标定流程（FR-017，PLC 协同）。在 worker 线程中执行。
		bool startAutoCalibration(std::string* errorMsg = nullptr);
		void stopAutoCalibration();

	signals:
		void calibrationProgress(int currentPoint, int totalPoints);
		void calibrationFinished(bool success, const QString& message);

	private:
		inf::infrastructure& inf_;
		std::atomic_bool stopRequested_{ false };
		std::thread worker_;
		std::vector<Point2D> mechanicalCoords_;

		void runAutoCalibration();

	public:
		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;
	};
}
