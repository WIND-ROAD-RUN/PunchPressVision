#pragma once

namespace global
{
	// 标定参数就绪性（FR-004、FR-006、FR-009、FR-015）
	struct CalibReadiness
	{
		bool distortionReady{ false };   // 畸变矫正参数就绪
		bool ninePointReady{ false };    // 九点标定参数就绪
		bool spliceReady{ false };       // 双相机拼接参数就绪

		bool allReady() const noexcept
		{
			return distortionReady && ninePointReady && spliceReady;
		}
	};

	// 生产就绪性（FR-009）
	struct ProductionReadiness
	{
		CalibReadiness calib;            // 标定就绪性
		bool hasLoadedModel{ false };    // 至少一个模型已加载
		bool plcConnected{ false };      // PLC 已连接

		bool allReady() const noexcept
		{
			return calib.allReady() && hasLoadedModel && plcConnected;
		}
	};
}
