#pragma once

#include <map>
#include <shared_mutex>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	/// <summary>
	/// 套版（StampPattern）业务封装。
	/// 套版是"要冲的图案外形"参考图（带透明通道的 PNG），叠加显示在材料图上，
	/// 用于辅助定义中心点、以及验证识别后的中心点是否确为图案中心。
	/// 套版是独立图库，不参与定位计算（定位仍由模板匹配完成）。
	/// </summary>
	class StampPatternBun
		: public QObject, public global::IBusiness
	{
		Q_OBJECT
	public:
		explicit StampPatternBun(inf::infrastructure& inf);

		// ---------- 图库 CRUD（转发到基础设施 StampPatternModule） ----------
		/// <summary>导入套版文件（图片原始字节复制保留透明通道；CAD DXF 图纸渲染为 RGBA 图片）。失败返回空 info（id 为空）。</summary>
		Config::StampPatternInfo importPattern(const std::string& sourceImagePath, const std::string& name);
		/// <summary>删除套版。若已加载则先卸载。</summary>
		void deletePattern(const std::string& id);
		/// <summary>重命名套版。</summary>
		void renamePattern(const std::string& id, const std::string& newName);
		/// <summary>更新对齐参数 / 透明度等数据（不改动图片文件），并同步已加载内存副本。</summary>
		void updatePatternData(const std::string& id, const Config::StampPatternData& data);

		// ---------- 查询 ----------
		std::vector<Config::StampPatternInfo> getAllPatterns() const;
		Config::StampPatternItem getPatternItem(const std::string& id) const;

		// ---------- 加载 / 卸载（叠加显示需要的内存态） ----------
		/// <summary>加载套版到内存。返回是否成功。</summary>
		bool loadPattern(const std::string& id);
		/// <summary>卸载指定套版。返回是否确实移除。</summary>
		bool unloadPattern(const std::string& id);
		/// <summary>卸载全部套版。</summary>
		void unloadAllPatterns();
		bool isPatternLoaded(const std::string& id) const;
		std::vector<std::string> getLoadedPatternIds() const;
		int getLoadedPatternCount() const;
		/// <summary>读取已加载套版的数据副本。未加载返回 false。</summary>
		bool getLoadedPatternData(const std::string& id, Config::StampPatternData& out) const;
		/// <summary>读取套版数据副本（供生产叠加）。未加载时自动从磁盘加载。失败返回 false。</summary>
		bool getPatternData(const std::string& id, Config::StampPatternData& out);

		// ---------- 对齐变换与叠加合成 ----------
		/// <summary>
		/// 构建手动对齐变换 A：套版图坐标 -> 参考(训练)图像坐标。
		/// 顺序为 缩放 -> 旋转 -> 平移（均围绕套版图原点），与 Halcon hom_mat2d_* 前乘语义一致。
		/// </summary>
		static HalconCpp::HTuple buildAlignHomMat2D(const Config::StampPatternData& data);

		/// <summary>
		/// 将 RGBA 套版图按 H_pat2base（套版图坐标 -> 基图坐标）变换并 alpha 混合叠加到基图上。
		/// 基图支持灰度(1 通道)或 RGB(3 通道)；套版图需为 RGBA(4 通道)，否则原样返回基图。
		/// alpha 为整体透明度 0~255，与套版图自身 alpha 通道相乘。
		/// 合成失败（Halcon 异常）时原样返回基图，不抛异常。
		/// </summary>
		static HalconCpp::HImage compositeOverlay(const HalconCpp::HImage& base,
			const HalconCpp::HImage& patternRGBA, const HalconCpp::HTuple& H_pat2base, int alpha);

		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;

	signals:
		/// 图库增删改后发出，通知 UI 刷新列表。
		void patternListChanged();
		/// 单个套版加载完成。
		void patternLoaded(const QString& patternId);
		/// 单个套版卸载完成。
		void patternUnloaded(const QString& patternId);
		/// 全部套版卸载完成。
		void patternsUnloaded();

	private:
		inf::infrastructure& inf_;
		// 已加载的套版集合（key = 套版 id）。叠加显示按需读取。
		std::map<std::string, Config::StampPatternData> loadedPatterns_;
		mutable std::shared_mutex patternMutex_;
	};
}
