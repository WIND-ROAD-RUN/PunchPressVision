#pragma once

namespace global
{
	// 相机索引
	enum class CameraIndex
	{
		Camera1,    // 左相机 / 相机 A
		Camera2     // 右相机 / 相机 B
	};

	// 运行模式（App 层）
	// 注意：畸变矫正、九点标定、双相机拼接已下沉至 infTool 层，不再作为 App 层模式。
	// 相关功能由独立工具程序提供，标定结果被主应用生产模式消费。
	enum class RunMode
	{
		Idle,               // 空闲/停止状态：相机为触发模式且不取流，业务层不处理图像
		Debug,              // 调试模式（FR-005 ~ FR-007）
		Production,         // 工作模式（FR-008 ~ FR-010）
		CreateModel,        // 创建模型模式：相机自由运行取流，图像实时刷新到模型编辑器
		DrawMatchRegion     // 绘制匹配范围模式：相机自由运行取流，用户在主界面绘制模板匹配的搜索范围
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
