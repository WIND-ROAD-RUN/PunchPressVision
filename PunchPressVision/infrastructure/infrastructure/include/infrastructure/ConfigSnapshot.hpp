#pragma once

namespace inf
{
	// 启动构建前调用：主数据目录中的关键配置文件若丢失/损坏（不存在、为空、JSON 截断），
	// 则从配置快照目录（global::path::configSnapshotDir()）逐个恢复；
	// ShapeModels 仅在整目录丢失或为空时整树恢复，避免复活用户有意删除的单个模型。
	void restoreConfigSnapshotIfNeeded();

	// 各配置模块加载成功后调用：将当前配置增量同步到快照目录。
	// 只增不删，快照始终保留最近一次成功加载时的完整配置；
	// 跳过 .tmp 临时残留与各模块内部 backup/ 目录（快照本身即第三级备份）。
	void updateConfigSnapshot();
}
