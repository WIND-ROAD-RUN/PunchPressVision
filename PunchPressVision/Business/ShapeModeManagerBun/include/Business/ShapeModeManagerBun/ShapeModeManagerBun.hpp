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
		HalconCpp::HImage trainingImage;       // 预处理后的图像（用于模型训练）
		HalconCpp::HImage rawImage;            // 原始拼接图（未经预处理，用于存储/显示）
		HalconCpp::HObject roi;              // ROI 区域（合并后的 HObject）
		HalconCpp::HObject mask;             // 屏蔽区域（合并后的 HObject）
		std::vector<HalconCpp::HObject> _paintCreateRoiList;   // ROI 列表（逐个保存，支持回撤）
		std::vector<HalconCpp::HObject> _paintShieldRoiList;  // Mask 列表（逐个保存，支持回撤）
		QPointF centerPoint;                 // 手动指定的中心点（可选）
		bool hasCenterPoint{ false };        // 是否使用手动中心点
		QString name;                        // 模型名称（空则用时间戳）
		double exposure{ 0.0 };
		double gain{ 0.0 };
		double exposure2{ 0.0 };   // Camera2
		double gain2{ 0.0 };       // Camera2
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

		// 训练参数（角度制）
		double angleStart{ -45.0 };
		double angleExtent{ 90.0 };
		int contrast{ 30 };
		int minContrast{ 10 };
		bool contrastAuto{ true };           // true = rbtn_auto 选中，Halcon 自动确定对比度
		double minScore{ 0.5 };
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
		double realX{ 0.0 };

		double realY{ 0.0 };

		double offsetX{ 0.0 };
		double offsetY{ 0.0 };
		bool found{ false };
		HalconCpp::HObject matchedContours;  // 匹配后变换到位的轮廓 XLD（用于主界面显示）
		HalconCpp::HImage preprocessedImage; // 预处理后的图像（通道提取+形态学运算，用于主界面显示）
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
	/// 已加载模型的公开信息（供上层绘制屏蔽区域等，不暴露 Halcon 句柄）。
	/// </summary>
	struct LoadedModelInfo
	{
		std::string modelId;
		std::string modelName;
		bool hasMask{ false };
		HalconCpp::HObject maskRegion;
	};

	/// <summary>
	/// 训练时使用的匹配区域配置快照（所有值复制，线程安全）。
	/// 支持多个识别范围。
	/// </summary>
	struct MatchRegionCfg
	{
		bool valid{ false };
		std::vector<inf::MatchRegionRect> regions;
		/// <summary>将所有区域 Union 合并为单个 Halcon 区域对象。</summary>
		HalconCpp::HObject unionRegion() const;
	};

	/// <summary>
	/// 静态训练函数的返回值。包含完整的 ShapeModelData 和轮廓 XLD。
	/// 纯数据类型，可安全跨线程传递。
	/// </summary>
	struct TrainShapeModelResult
	{
		bool   success{ false };
		std::string errorMsg;
		Config::ShapeModelData data;
		HalconCpp::HObject contours;
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

	/// <summary>
	/// 用户可配置的模板匹配参数。
	/// 值从 ShapeModelData 读取，通过 model_params.txt 持久化。
	/// angleStart/angleExtent 使用弧度（与 Halcon 一致）。
	/// </summary>
	struct ModelMatchParams
	{
		int numMatches{ 1 };
		double minScore{ 0.5 };
		double angleStart{ -45.0 };  // 角度制，搜索起始角度
		double angleExtent{ 90.0 };  // 角度制，搜索角度范围（-45°~45°）
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

		/// <summary>纯静态训练函数。无 QObject / 基础设施依赖，可从任意线程调用。</summary>
		static TrainShapeModelResult trainShapeModel(
			const CreateModelRequest& req,
			const MatchRegionCfg& matchRegion);

		/// <summary>将训练结果持久化为新模型（主线程）。发射 modelContoursFound + modelListChanged。</summary>
		bool persistNewModel(
			TrainShapeModelResult& result,
			const CreateModelRequest& req,
			Config::ShapeModelInfo& outInfo,
			std::string* errorMsg = nullptr);

		/// <summary>将训练结果更新到已有模型（主线程）。保留旧 offset / 匹配参数。</summary>
		bool persistUpdatedModel(
			TrainShapeModelResult& result,
			const std::string& id,
			std::string* errorMsg = nullptr);

		// 模型加载/卸载（多模型支持）
		/// <summary>批量加载模型，全量替换已加载集合。部分失败不中断其余加载。</summary>
		/// <returns>全部成功返回 true；否则返回 false，失败 ID 放入 failedIds。</returns>
		bool loadModels(const std::vector<std::string>& ids,
			std::vector<std::string>* failedIds = nullptr,
			std::string* errorMsg = nullptr);
		/// <summary>加载单个模型（兼容旧接口），内部委托 loadModels({id})。</summary>
		bool loadModel(const std::string& id, std::string* errorMsg = nullptr);
		/// <summary>
		/// 将指定模型的曝光/增益写入相机硬件（Camera1 + Camera2 统一设置）。
		/// 用于加载模型时恢复创建时的相机参数。
		/// </summary>
		bool applyModelCameraSettings(const std::string& modelId,
			std::string* errorMsg = nullptr);
		/// <summary>卸载所有已加载模型并释放 Halcon 句柄。</summary>
		void unloadAllModels();
		/// <summary>卸载指定模型。返回 true 表示成功移除。</summary>
		bool unloadModel(const std::string& id);
		/// <summary>卸载所有模型（兼容旧接口）。</summary>
		void unloadCurrentModel();
		bool isModelLoaded() const;
		std::string currentModelId() const;
		std::vector<std::string> getLoadedModelIds() const;
		int getLoadedModelCount() const;
		std::vector<LoadedModelInfo> getLoadedModelInfos() const;

		// 模型偏移量（FR-035）
		/// <summary>获取指定模型的用户偏移量。</summary>
		ModelUserOffset getUserOffset(const std::string& modelId) const;
		/// <summary>设置指定模型的用户偏移量，更新内存并持久化到磁盘。</summary>
		bool setUserOffset(const std::string& modelId,
			double offsetX, double offsetY, double offsetAngle,
			std::string* errorMsg = nullptr);

		// 模板匹配参数（查找数量、最低分数、角度范围）
		/// <summary>获取指定模型的匹配参数。</summary>
		ModelMatchParams getMatchParams(const std::string& modelId) const;
		/// <summary>设置指定模型的匹配参数，更新内存并持久化到磁盘。</summary>
		bool setMatchParams(const std::string& modelId,
			int numMatches, double minScore, double angleStart, double angleExtent,
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
		/// 匹配参数已更新，通知 UI 刷新
		void matchParamsChanged(const QString& modelId);

	private:
		inf::infrastructure& inf_;
		infTool::infTool& inf_tool_;


		// 对图像应用与创建模板时相同的预处理（通道提取、开/闭运算、均值滤波）
		HalconCpp::HImage preprocessImage(const HalconCpp::HImage& image,
			const Config::ShapeModelData& data) const;

		// 已加载的模型集合（加载顺序即匹配优先级）
		std::vector<LoadedModel> loadedModels_;
		mutable std::shared_mutex modelCacheMutex_;

		// 持久化：保存/恢复上次加载的模型 ID 列表
		void saveLastLoadedModels();
		void loadLastLoadedModels();
	};
}
