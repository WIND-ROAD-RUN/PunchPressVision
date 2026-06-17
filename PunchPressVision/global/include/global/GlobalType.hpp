#pragma once

namespace global
{
	// 相机索引
	enum class CameraIndex
	{
		Camera1,    // 左相机 / 相机 A
		Camera2     // 右相机 / 相机 B
	};

	// 运行模式
	enum class RunMode
	{
		Idle,               // 空闲/停止状态：相机为触发模式且不取流，业务层不处理图像
		Debug,              // 调试模式（FR-005 ~ FR-007）
		Production,         // 工作模式（FR-008 ~ FR-010）
		CalibDistortion,    // 畸变矫正模式（FR-011 ~ FR-014）
		CalibNinePoint,     // 九点标定模式（FR-015 ~ FR-018）
		Splice,             // 双相机拼接模式（FR-019 ~ FR-020）
		CreateModel         // 创建模型模式：相机自由运行取流，图像实时刷新到模型编辑器
	};

	// 触发源
	enum class TriggerSource
	{
		Software,   // 软触发
		Line0,      // 硬件触发线 0（工作模式使用）
		Line1,
		Line2
	};

	// 光源状态
	enum class LightState
	{
		Off,
		On
	};
}
