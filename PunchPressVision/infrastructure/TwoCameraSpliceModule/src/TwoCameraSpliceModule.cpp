#include "infrastructure/TwoCameraSpliceModule/TwoCameraSpliceModule.hpp"

#include "infrastructure/TwoCameraSpliceModule/TwoCameraSpliceModulePath.hpp"

#include <QDebug>

namespace inf
{
	void TwoCameraSpliceModule::build()
	{
		twoCameraSpliceConfig.loadInDir(TwoCameraSpliceModulePath.RootPath);
	}

	void TwoCameraSpliceModule::destroy()
	{
		save();
	}

	void TwoCameraSpliceModule::save()
	{
		// 未加载/未标定时 MapSingle1 未初始化，此时保存会把磁盘上
		// 已有的 map/图片文件全部清除（writeObjectSafe 未初始化即删文件），
		// 导致历史标定数据丢失，因此直接跳过。
		if (!twoCameraSpliceConfig.MapSingle1.IsInitialized())
		{
			qWarning() << "[TwoCameraSpliceModule] MapSingle1未初始化，跳过保存，避免清除磁盘已有拼接参数";
			return;
		}
		twoCameraSpliceConfig.saveInDir(TwoCameraSpliceModulePath.RootPath);
	}
}
