#pragma once

namespace global
{
	// 定位结果（FR-010）
	struct PositionResult
	{
		double offsetX{ 0.0 };   // X 轴偏移（mm，世界坐标）
		double offsetY{ 0.0 };   // Y 轴偏移（mm，世界坐标）
		double angle{ 0.0 };     // 旋转角度（度）
		double score{ 0.0 };     // 匹配得分（0.0 ~ 1.0）
		bool valid{ false };     // 结果有效标志
	};
}
