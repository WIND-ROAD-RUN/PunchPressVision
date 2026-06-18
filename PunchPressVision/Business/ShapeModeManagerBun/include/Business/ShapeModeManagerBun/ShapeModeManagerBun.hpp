#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QPointF>
#include <QRectF>
#include <QVector>

#include "global/GlobalInterface.hpp"
#include "infrastructure/infrastructure.hpp"
#include "infTool/infTool.hpp"

#include "halconcpp/HalconCpp.h"

namespace bun
{
	// 创建模型请求（FR-024）
	struct CreateModelRequest
	{
		HalconCpp::HImage trainingImage;
		HalconCpp::HObject roi;              // ROI 区域（合并后的 HObject）
		HalconCpp::HObject mask;             // 屏蔽区域（合并后的 HObject）
		std::vector<HalconCpp::HObject> _paintCreateRoiList;   // ROI 列表（逐个保存，支持回撤）
		std::vector<HalconCpp::HObject> _paintShieldRoiList;  // Mask 列表（逐个保存，支持回撤）
		QPointF centerPoint;                 // 手动指定的中心点（可选）
		bool hasCenterPoint{ false };        // 是否使用手动中心点
		QString name;                        // 模型名称（空则用时间戳）
		double exposure{ 0.0 };
		double gain{ 0.0 };
		bool upperLight{ false };
		bool lowerLight{ false };

		// 图像预处理参数
		int imageChannelType{ 0 };           // comboBox_ImageType index
		bool useOpening{ false };
		int openingSize{ 5 };
		bool useClosing{ false };
		int closingSize{ 5 };
		bool useMean{ false };
		int meanSize{ 5 };

		// 训练参数
		int numLevels{ 4 };
		double angleStart{ -3.14 };
		double angleExtent{ 6.28 };
		double angleStep{ 0.0 };             // 0 = auto
		QString optimization{ "auto" };
		QString metric{ "use_polarity" };
		int contrast{ 30 };
		int minContrast{ 10 };
	};

	// 匹配结果（FR-010）
	struct MatchResult
	{
		std::string modelId;       // 匹配到的模型 ID（多模型场景）
		std::string modelName;     // 匹配到的模型名称
		double row{ 0.0 };
		double column{ 0.0 };
		double angle{ 0.0 };
		double score{ 0.0 };
		double offsetX{ 0.0 };
		double offsetY{ 0.0 };
		bool found{ false };
	};

	/// <summary>
	/// 已加载到内存中的模型条目。
	/// 包含 Halcon 句柄及训练参数，供生产匹配使用。
	/// </summary>
	struct LoadedModel
	{
		std::string modelId;
		std::string modelName;
		HalconCpp::HTuple handle;
		Config::ShapeModelData data;
	};

	/// <summary>
	/// 用户可配置的模型偏移量。
	/// 值从 ShapeModelData.offsetX/Y/Angle 读取，通过 model_params.txt 持久化。
	/// </summary>
	struct ModelUserOffset
	{
		double offsetX{ 0.0 };
		double offsetY{ 0.0 };
		double offsetAngle{ 0.0 };
	};

	class ShapeModeManagerBun
		: public QObject, public global::IBusiness
	{
		Q_OBJECT
	public:
		explicit ShapeModeManagerBun(inf::infrastructure& inf, infTool::infTool& infTool);

		// 模型 CRUD（FR-024 ~ FR-030）
		bool createModel(const CreateModelRequest& req,
			Config::ShapeModelInfo& outInfo,
			std::string* errorMsg = nullptr);
		bool updateModel(const std::string& id, const CreateModelRequest& req,
			std::string* errorMsg = nullptr);
		bool deleteModel(const std::string& id, std::string* errorMsg = nullptr);
		bool renameModel(const std::string& id, const QString& newName,
			std::string* errorMsg = nullptr);

		// 模型加载/卸载（多模型支持）
		/// <summary>批量加载模型，全量替换已加载集合。部分失败不中断其余加载。</summary>
		/// <returns>全部成功返回 true；否则返回 false，失败 ID 放入 failedIds。</returns>
		bool loadModels(const std::vector<std::string>& ids,
			std::vector<std::string>* failedIds = nullptr,
			std::string* errorMsg = nullptr);
		/// <summary>加载单个模型（兼容旧接口），内部委托 loadModels({id})。</summary>
		bool loadModel(const std::string& id, std::string* errorMsg = nullptr);
		/// <summary>卸载所有已加载模型并释放 Halcon 句柄。</summary>
		void unloadAllModels();
		/// <summary>卸载所有模型（兼容旧接口）。</summary>
		void unloadCurrentModel();
		bool isModelLoaded() const;
		std::string currentModelId() const;
		std::vector<std::string> getLoadedModelIds() const;
		int getLoadedModelCount() const;

		// 模型偏移量（FR-035）
		/// <summary>获取指定模型的用户偏移量。</summary>
		ModelUserOffset getUserOffset(const std::string& modelId) const;
		/// <summary>设置指定模型的用户偏移量，更新内存并持久化到磁盘。</summary>
		bool setUserOffset(const std::string& modelId,
			double offsetX, double offsetY, double offsetAngle,
			std::string* errorMsg = nullptr);

		// 模板匹配推理（FR-010）— 遍历所有已加载模型，返回匹配到的结果集
		/// <summary>
		/// 对图像遍历所有已加载模型进行匹配。
		/// 返回所有 hit 的结果（可能为空），按 loadedModels_ 顺序排列。
		/// </summary>
		std::vector<MatchResult> match(const HalconCpp::HImage& image);

		// 临时创建模型并匹配测试（供创建界面"识别"按钮使用）
		MatchResult testRecognize(const CreateModelRequest& req,
			std::string* errorMsg = nullptr);

		// 查询
		std::vector<Config::ShapeModelInfo> getAllModels() const;
		std::vector<Config::ShapeModelInfo> searchModels(const QString& keyword) const;

		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;

	signals:
		void modelListChanged();
		/// @deprecated 多模型场景请使用 modelsLoaded
		void modelLoaded(const QString& modelName);
		/// @deprecated 多模型场景请使用 modelsUnloaded
		void modelUnloaded();
		/// 批量加载完成：模型名列表、成功数、失败数
		void modelsLoaded(const QStringList& modelNames, int successCount, int failCount);
		/// 所有模型已卸载
		void modelsUnloaded();
		/// 创建模型成功并提取到轮廓后发出，供 UI 显示
		void modelContoursFound(const HalconCpp::HObject& contours);
		/// 模型偏移量已更新，通知 UI 刷新
		void modelOffsetChanged(const QString& modelId);

	private:
		inf::infrastructure& inf_;
		infTool::infTool& inf_tool_;

		// 创建/更新模型共用的数据生成逻辑
		bool createModelInternal(const CreateModelRequest& req,
			Config::ShapeModelData& outData, std::string* errorMsg);

		// 已加载的模型集合（加载顺序即匹配优先级）
		std::vector<LoadedModel> loadedModels_;
		mutable std::shared_mutex modelCacheMutex_;
	};
}
