#pragma once

#include <string>

namespace global
{
	// 定位结果（FR-010）
	struct PositionResult
	{
		double offsetX{ 0.0 };   // X 轴偏移（mm，世界坐标）
		double offsetY{ 0.0 };   // Y 轴偏移（mm，世界坐标）
		double angle{ 0.0 };     // 旋转角度（度）
		// 中心点2 的第二组位置输出（mm/度；模板未定义中心点2时回退为组1的值）
		double offset2X{ 0.0 };
		double offset2Y{ 0.0 };
		double angle2{ 0.0 };
		bool hasCenter2{ false };  // 模板是否定义了中心点2
		double score{ 0.0 };     // 匹配得分（0.0 ~ 1.0）
		bool valid{ false };     // 结果有效标志
		std::string modelId;     // 匹配到的模型 ID（多模型场景）
		std::string modelName;   // 匹配到的模型名称
	};
}
