#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

namespace bun
{
	ShapeModeManagerBun::ShapeModeManagerBun(inf::infrastructure& inf, infTool::infTool& infTool)
		: inf_(inf)
		, inf_tool_(infTool)
	{
	}

	bool ShapeModeManagerBun::createModel(const CreateModelRequest& req,
		Config::ShapeModelInfo& outInfo,
		std::string* errorMsg)
	{
		using namespace HalconCpp;
		try
		{
			if (!req.trainingImage.IsInitialized())
			{
				if (errorMsg) *errorMsg = "训练图像未初始化";
				return false;
			}

			// 1. 在 ROI 内裁剪模板图（若提供了 ROI）
			HImage templateImage = req.trainingImage;
			double centerX = 0.0, centerY = 0.0;
			if (req.roi.IsInitialized())
			{
				HTuple area, row, col;
				AreaCenter(req.roi, &area, &row, &col);
				if (area.Length() > 0 && area[0].D() > 0)
				{
					centerY = row[0].D();
					centerX = col[0].D();
					HObject reduced;
					ReduceDomain(req.trainingImage, req.roi, &reduced);
					templateImage = HImage(reduced);
				}
			}

			// 若提供了屏蔽区域，从 ROI 中剔除
			if (req.mask.IsInitialized())
			{
				HObject diff;
				Difference(req.roi, req.mask, &diff);
				HObject reduced;
				ReduceDomain(req.trainingImage, diff, &reduced);
				templateImage = HImage(reduced);
			}

			// 使用手动指定的中心点（优先于 ROI 形心）
			if (req.hasCenterPoint)
			{
				centerX = req.centerPoint.x();
				centerY = req.centerPoint.y();
			}

			// 2. 创建 Shape Model
			HTuple modelID;
			CreateShapeModel(templateImage,
				req.numLevels,
				HTuple(req.angleStart),
				HTuple(req.angleExtent),
				(req.angleStep > 0 ? HTuple(req.angleStep) : HTuple("auto")),
				req.optimization.toStdString().c_str(),
				req.metric.toStdString().c_str(),
				req.contrast,
				req.minContrast,
				&modelID);

			// 3. 准备模型数据
			Config::ShapeModelData data;
			data._templateMatImage = templateImage;
			data._originalImage = req.trainingImage;
			data.hv_ModelID = modelID;
			data._createModelExposureTime = req.exposure;
			data._createModelGain = req.gain;
			data.centerX = centerX;
			data.centerY = centerY;
			data.maxContrast = req.contrast;
			data.minContrast = req.minContrast;

			// 图像预处理参数
			data._SingleChannelType = req.imageChannelType;
			data._createModelPreProcessType = req.imageChannelType;
			data._createModelUseOpening = req.useOpening;
			data._createModelOpeningRadius = req.openingSize;
			data._createModelUseClosing = req.useClosing;
			data._createModelClosingRadius = req.closingSize;
			data._createModelUseMean = req.useMean;
			data._createModelMeanRadius = req.meanSize;

			// ROI 矩形列表（逐个保存为 HObject，支持回撤）
			data._paintCreateRoiList.clear();
			for (const auto& r : req.roiRects)
			{
				HalconCpp::HObject obj;
				HalconCpp::GenRectangle1(&obj,
					r.top(), r.left(), r.bottom(), r.right());
				data._paintCreateRoiList.push_back(obj);
			}

			// Mask 矩形列表（逐个保存为 HObject，支持回撤）
			data._paintShieldRoiList.clear();
			for (const auto& r : req.maskRects)
			{
				HalconCpp::HObject obj;
				HalconCpp::GenRectangle1(&obj,
					r.top(), r.left(), r.bottom(), r.right());
				data._paintShieldRoiList.push_back(obj);
			}

			// 生成标注图：在原始图像上叠加 ROI（白线）和 Mask（黑线）
			if (!req.roiRects.isEmpty() || !req.maskRects.isEmpty())
			{
				HImage annotated = req.trainingImage;
				// ROI → 白色边框（灰度值 255）
				for (const auto& r : req.roiRects)
				{
					HObject rectROI;
					GenRectangle1(&rectROI, r.top(), r.left(), r.bottom(), r.right());
					OverpaintRegion(annotated, rectROI, 255, "margin");
				}
				// Mask → 黑色边框（灰度值 0）
				for (const auto& r : req.maskRects)
				{
					HObject rectMask;
					GenRectangle1(&rectMask, r.top(), r.left(), r.bottom(), r.right());
					OverpaintRegion(annotated, rectMask, 0, "margin");
				}
				data._annotatedImage = annotated;
			}

			// 4. 元数据
			Config::ShapeModelInfo::BaseInfo baseInfo;
			baseInfo.name = req.name.isEmpty()
				? inf_.shape_model_manager_module_->getCurrentTime_yyMMddHHmmsszzz()
				: req.name.toStdString();

			// 5. 存储（生成 id/时间戳目录并落盘）
			outInfo = inf_.shape_model_manager_module_->addShapeModelItem(data, baseInfo);

			emit modelListChanged();
			return true;
		}
		catch (const HException& e)
		{
			if (errorMsg)
				*errorMsg = std::string("创建模型失败: ") + e.ErrorMessage().Text();
			return false;
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "创建模型发生未知错误";
			return false;
		}
	}

	bool ShapeModeManagerBun::deleteModel(const std::string& id, std::string* errorMsg)
	{
		try
		{
			{
				std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
				if (currentModelId_ == id)
				{
					currentModelId_.clear();
					currentModelHandle_ = HalconCpp::HTuple();
				}
			}
			inf_.shape_model_manager_module_->deleteShapeModelItem(id);
			emit modelListChanged();
			return true;
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "删除模型失败";
			return false;
		}
	}

	bool ShapeModeManagerBun::renameModel(const std::string& id, const QString& newName,
		std::string* errorMsg)
	{
		try
		{
			Config::ShapeModelInfo::BaseInfo baseInfo;
			// 保留其它元数据，仅更新名称
			auto item = inf_.shape_model_manager_module_->getShapeModelItem(id);
			baseInfo = item.info.base_info;
			baseInfo.name = newName.toStdString();
			inf_.shape_model_manager_module_->changeShapeModelItem(id, baseInfo);
			emit modelListChanged();
			return true;
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "重命名模型失败";
			return false;
		}
	}

	bool ShapeModeManagerBun::loadModel(const std::string& id, std::string* errorMsg)
	{
		using namespace HalconCpp;
		try
		{
			auto item = inf_.shape_model_manager_module_->getShapeModelItem(id);

			std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
			currentModelData_ = item.data;
			currentModelId_ = id;

			// 优先使用已加载的模型句柄；否则从 .shm 读取
			if (item.data.hv_ModelID.Length() > 0)
			{
				currentModelHandle_ = item.data.hv_ModelID;
			}
			else if (!item.data.modelPath.empty())
			{
				ReadShapeModel((item.data.modelPath + "/model.shm").c_str(), &currentModelHandle_);
			}
			else
			{
				currentModelId_.clear();
				if (errorMsg) *errorMsg = "模型文件缺失";
				return false;
			}

			emit modelLoaded(QString::fromStdString(item.info.base_info.name));
			return true;
		}
		catch (const HException& e)
		{
			if (errorMsg) *errorMsg = std::string("加载模型失败: ") + e.ErrorMessage().Text();
			return false;
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "加载模型发生未知错误";
			return false;
		}
	}

	void ShapeModeManagerBun::unloadCurrentModel()
	{
		std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
		currentModelId_.clear();
		currentModelHandle_ = HalconCpp::HTuple();
		emit modelUnloaded();
	}

	bool ShapeModeManagerBun::isModelLoaded() const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		return !currentModelId_.empty();
	}

	std::string ShapeModeManagerBun::currentModelId() const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		return currentModelId_;
	}

	MatchResult ShapeModeManagerBun::match(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;
		MatchResult result;

		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		if (currentModelId_.empty() || currentModelHandle_.Length() == 0)
			return result;
		if (!image.IsInitialized())
			return result;

		try
		{
			const double rotate = currentModelData_.rotateAngle;
			HTuple angleStart = HTuple(-rotate).TupleRad();
			HTuple angleExtent = HTuple(rotate * 2).TupleRad();

			HTuple row, column, angle, score;
			FindShapeModel(image,
				currentModelHandle_,
				angleStart,
				angleExtent,
				0.5,    // MinScore
				1,      // NumMatches
				0.5,    // MaxOverlap
				"least_squares",
				0,      // NumLevels(0=auto)
				0.9,    // Greediness
				&row, &column, &angle, &score);

			if (score.Length() > 0 && score[0].D() >= 0.5)
			{
				result.found = true;
				result.row = row[0].D();
				result.column = column[0].D();
				result.angle = angle[0].D();
				result.score = score[0].D();
				result.offsetX = result.column - currentModelData_.centerX;
				result.offsetY = result.row - currentModelData_.centerY;
			}
		}
		catch (...)
		{
			// 匹配失败 → found=false
		}
		return result;
	}

	MatchResult ShapeModeManagerBun::testRecognize(const CreateModelRequest& req,
		std::string* errorMsg)
	{
		using namespace HalconCpp;
		MatchResult result;

		if (!req.trainingImage.IsInitialized())
		{
			if (errorMsg) *errorMsg = "训练图像未初始化";
			return result;
		}

		try
		{
			// 1. 在 ROI 内裁剪模板图
			HImage templateImage = req.trainingImage;
			if (req.roi.IsInitialized())
			{
				HObject reduced;
				ReduceDomain(req.trainingImage, req.roi, &reduced);
				templateImage = HImage(reduced);
			}
			if (req.mask.IsInitialized())
			{
				HObject diff;
				Difference(req.roi, req.mask, &diff);
				HObject reduced;
				ReduceDomain(req.trainingImage, diff, &reduced);
				templateImage = HImage(reduced);
			}

			// 2. 临时创建 Shape Model
			HTuple modelID;
			CreateShapeModel(templateImage,
				req.numLevels,
				HTuple(req.angleStart),
				HTuple(req.angleExtent),
				(req.angleStep > 0 ? HTuple(req.angleStep) : HTuple("auto")),
				req.optimization.toStdString().c_str(),
				req.metric.toStdString().c_str(),
				req.contrast, req.minContrast,
				&modelID);

			// 3. 匹配测试
			HTuple row, column, angle, score;
			FindShapeModel(req.trainingImage, modelID,
				HTuple(req.angleStart), HTuple(req.angleExtent),
				0.3, 1, 0.5, "least_squares", 0, 0.9,
				&row, &column, &angle, &score);

			if (score.Length() > 0 && score[0].D() >= 0.3)
			{
				result.found = true;
				result.row = row[0].D();
				result.column = column[0].D();
				result.angle = angle[0].D();
				result.score = score[0].D();
			}

			// 4. 清理临时模型
			ClearShapeModel(modelID);
		}
		catch (const HException& e)
		{
			if (errorMsg)
				*errorMsg = std::string("识别测试失败: ") + e.ErrorMessage().Text();
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "识别测试发生未知错误";
		}

		return result;
	}

	std::vector<Config::ShapeModelInfo> ShapeModeManagerBun::getAllModels() const
	{
		return inf_.shape_model_manager_module_->getShapeModelInfos();
	}

	std::vector<Config::ShapeModelInfo> ShapeModeManagerBun::searchModels(const QString& keyword) const
	{
		std::vector<Config::ShapeModelInfo> result;
		const std::string kw = keyword.toStdString();
		for (const auto& info : inf_.shape_model_manager_module_->getShapeModelInfos())
		{
			if (kw.empty() || info.base_info.name.find(kw) != std::string::npos)
				result.push_back(info);
		}
		return result;
	}

	void ShapeModeManagerBun::build()
	{
		if (inf_.shape_model_manager_module_)
			inf_.shape_model_manager_module_->readAllShapeModelInfos();
	}

	void ShapeModeManagerBun::destroy()
	{
		unloadCurrentModel();
	}

	void ShapeModeManagerBun::start()
	{
	}

	void ShapeModeManagerBun::stop()
	{
	}
}
