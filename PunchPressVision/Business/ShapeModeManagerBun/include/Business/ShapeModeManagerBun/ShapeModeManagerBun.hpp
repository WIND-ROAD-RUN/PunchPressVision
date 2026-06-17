#pragma once

#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include <QObject>
#include <QString>
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
		QVector<QRectF> roiRects;            // ROI 矩形列表（逐个保存，支持回撤）
		QVector<QRectF> maskRects;           // Mask 矩形列表（逐个保存，支持回撤）
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
		double row{ 0.0 };
		double column{ 0.0 };
		double angle{ 0.0 };
		double score{ 0.0 };
		double offsetX{ 0.0 };
		double offsetY{ 0.0 };
		bool found{ false };
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
		bool deleteModel(const std::string& id, std::string* errorMsg = nullptr);
		bool renameModel(const std::string& id, const QString& newName,
			std::string* errorMsg = nullptr);

		// 模型加载/卸载
		bool loadModel(const std::string& id, std::string* errorMsg = nullptr);
		void unloadCurrentModel();
		bool isModelLoaded() const;
		std::string currentModelId() const;

		// 模板匹配推理（FR-010）
		MatchResult match(const HalconCpp::HImage& image);

		// 查询
		std::vector<Config::ShapeModelInfo> getAllModels() const;
		std::vector<Config::ShapeModelInfo> searchModels(const QString& keyword) const;

		void build() override;
		void destroy() override;
		void start() override;
		void stop() override;

	signals:
		void modelListChanged();
		void modelLoaded(const QString& modelName);
		void modelUnloaded();

	private:
		inf::infrastructure& inf_;
		infTool::infTool& inf_tool_;

		// 当前加载的模型缓存
		std::string currentModelId_;
		HalconCpp::HTuple currentModelHandle_;
		Config::ShapeModelData currentModelData_;
		mutable std::shared_mutex modelCacheMutex_;
	};
}
