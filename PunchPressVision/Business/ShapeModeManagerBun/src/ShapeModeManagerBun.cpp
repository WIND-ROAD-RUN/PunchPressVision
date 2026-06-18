#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

#include <cstdlib>
#include <filesystem>

namespace bun
{
	ShapeModeManagerBun::ShapeModeManagerBun(inf::infrastructure& inf, infTool::infTool& infTool)
		: inf_(inf)
		, inf_tool_(infTool)
	{
	}

	bool ShapeModeManagerBun::createModelInternal(const CreateModelRequest& req,
		Config::ShapeModelData& outData, std::string* errorMsg)
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
			HalconCpp::CreateShapeModel(
				templateImage,
				"auto",
				0, HalconCpp::HTuple(360).TupleRad(),
				"auto",
				"auto",
				"use_polarity",
				"auto",
				"auto",
				&modelID);

			// 3. 使用刚创建的模板在原图上验证匹配，作为创建成功的依据
			HTuple matchRow, matchCol, matchAngle, matchScore;
			FindShapeModel(templateImage,
				modelID,
				HTuple(req.angleStart),
				HTuple(req.angleExtent),
				0.5,    // MinScore
				1,      // NumMatches
				0.5,    // MaxOverlap
				"least_squares",
				(req.numLevels > 0 ? HTuple(req.numLevels) : HTuple("auto")),
				0.9,    // Greediness
				&matchRow, &matchCol, &matchAngle, &matchScore);

			if (matchScore.Length() == 0 || matchScore[0].D() < 0.5)
			{
				ClearShapeModel(modelID);
				if (errorMsg) *errorMsg = "模板创建后无法在图像中匹配到自身，请调整 ROI 或训练参数";
				return false;
			}

			// 找到模型中心，作为模板中心点
			centerX = matchCol[0].D();
			centerY = matchRow[0].D();

			// 4. 准备模型数据
			outData._templateMatImage = templateImage;
			outData._originalImage = req.trainingImage;
			outData.hv_ModelID = modelID;
			outData._createModelExposureTime = req.exposure;
			outData._createModelGain = req.gain;
			outData.upperLight = req.upperLight;
			outData.lowerLight = req.lowerLight;
			outData.centerX = centerX;
			outData.centerY = centerY;

			// 图像预处理参数
			outData._createModelPreProcessType = req.imageChannelType;
			outData._createModelUseOpening = req.useOpening;
			outData._createModelOpeningRadius = req.openingSize;
			outData._createModelUseClosing = req.useClosing;
			outData._createModelClosingRadius = req.closingSize;
			outData._createModelUseMean = req.useMean;
			outData._createModelMeanRadius = req.meanSize;

			// 训练参数
			outData.numLevels = req.numLevels;
			outData.angleStart = req.angleStart;
			outData.angleExtent = req.angleExtent;
			outData.angleStep = req.angleStep;
			outData.optimization = req.optimization.toStdString();
			outData.metric = req.metric.toStdString();
			outData.contrast = req.contrast;
			outData.minContrast = req.minContrast;

			// ROI 列表（逐个保存为 HObject，支持回撤）
			outData._paintCreateRoiList = req._paintCreateRoiList;

			// Mask 列表（逐个保存为 HObject，支持回撤）
			outData._paintShieldRoiList = req._paintShieldRoiList;

			// 生成标注图：在原始图像上叠加 ROI（白线）和 Mask（黑线）
			if (!req._paintCreateRoiList.empty() || !req._paintShieldRoiList.empty())
			{
				HImage annotated = req.trainingImage;
				// ROI → 白色边框（灰度值 255）
				for (const auto& obj : req._paintCreateRoiList)
				{
					if (obj.IsInitialized())
						OverpaintRegion(annotated, obj, 255, "margin");
				}
				// Mask → 黑色边框（灰度值 0）
				for (const auto& obj : req._paintShieldRoiList)
				{
					if (obj.IsInitialized())
						OverpaintRegion(annotated, obj, 0, "margin");
				}
				outData._annotatedImage = annotated;
			}

			// 5. 提取模型轮廓并变换到匹配位置，供 UI 显示
			try
			{
				HObject modelContours;
				GetShapeModelContours(&modelContours, modelID, 1);

				HTuple homMat2D;
				VectorAngleToRigid(0, 0, 0, matchRow[0], matchCol[0], matchAngle[0], &homMat2D);

				HObject transformedContours;
				AffineTransContourXld(modelContours, &transformedContours, homMat2D);

				outData._findCreateXldObj = transformedContours;
				emit modelContoursFound(transformedContours);
			}
			catch (...) {}

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

	bool ShapeModeManagerBun::createModel(const CreateModelRequest& req,
		Config::ShapeModelInfo& outInfo, std::string* errorMsg)
	{
		Config::ShapeModelData data;
		if (!createModelInternal(req, data, errorMsg))
			return false;

		// 元数据
		Config::ShapeModelInfo::BaseInfo baseInfo;
		baseInfo.name = req.name.isEmpty()
			? inf_.shape_model_manager_module_->getCurrentTime_yyMMddHHmmsszzz()
			: req.name.toStdString();

		// 存储（生成 id/时间戳目录并落盘）
		outInfo = inf_.shape_model_manager_module_->addShapeModelItem(data, baseInfo);

		emit modelListChanged();
		return true;
	}

	bool ShapeModeManagerBun::updateModel(const std::string& id,
		const CreateModelRequest& req, std::string* errorMsg)
	{
		Config::ShapeModelData data;
		if (!createModelInternal(req, data, errorMsg))
			return false;

		inf_.shape_model_manager_module_->changeShapeModelItem(id, data);

		emit modelListChanged();
		return true;
	}

	bool ShapeModeManagerBun::deleteModel(const std::string& id, std::string* errorMsg)
	{
		try
		{
			{
				std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
				// 从已加载列表中移除（若存在）
				loadedModels_.erase(
					std::remove_if(loadedModels_.begin(), loadedModels_.end(),
						[&id](const LoadedModel& m) { return m.modelId == id; }),
					loadedModels_.end());
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

	// ===== 多模型加载/卸载 =====

	bool ShapeModeManagerBun::loadModels(const std::vector<std::string>& ids,
		std::vector<std::string>* failedIds, std::string* errorMsg)
	{
		using namespace HalconCpp;
		if (ids.empty())
		{
			// 空列表 = 卸载全部
			unloadAllModels();
			return true;
		}

		int successCount = 0;
		int failCount = 0;
		QStringList loadedNames;

		{
			std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
			// 全量替换：先清空旧的（析构 HTuple 自动释放 Halcon 资源）
			loadedModels_.clear();

			for (const auto& id : ids)
			{
				try
				{
					auto item = inf_.shape_model_manager_module_->getShapeModelItem(id);

					LoadedModel lm;
					lm.modelId = id;
					lm.modelName = item.info.base_info.name;
					lm.data = item.data;

					// 优先使用内存中的句柄；否则从 .shm 读取
					if (item.data.hv_ModelID.Length() > 0)
					{
						lm.handle = item.data.hv_ModelID;
					}
					else if (!item.data.modelPath.empty())
					{
						ReadShapeModel((item.data.modelPath + "/model.shm").c_str(), &lm.handle);
					}
					else
					{
						if (failedIds) failedIds->push_back(id);
						++failCount;
						continue;
					}

					loadedModels_.push_back(std::move(lm));
					loadedNames.append(QString::fromStdString(lm.modelName));
					++successCount;
				}
				catch (const HException& e)
				{
					if (failedIds) failedIds->push_back(id);
					if (errorMsg)
						*errorMsg = std::string("加载模型失败: ") + e.ErrorMessage().Text();
					++failCount;
				}
				catch (...)
				{
					if (failedIds) failedIds->push_back(id);
					if (errorMsg)
						*errorMsg = "加载模型发生未知错误";
					++failCount;
				}
			}
		}

		// 发出信号（兼容旧 + 新）
		if (!loadedNames.isEmpty())
			emit modelLoaded(loadedNames.first());
		emit modelsLoaded(loadedNames, successCount, failCount);

		return failCount == 0;
	}

	bool ShapeModeManagerBun::loadModel(const std::string& id, std::string* errorMsg)
	{
		return loadModels({id}, nullptr, errorMsg);
	}

	void ShapeModeManagerBun::unloadAllModels()
	{
		{
			std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
			loadedModels_.clear();  // HTuple 析构自动释放 Halcon 句柄
		}
		emit modelUnloaded();
		emit modelsUnloaded();
	}

	void ShapeModeManagerBun::unloadCurrentModel()
	{
		unloadAllModels();
	}

	bool ShapeModeManagerBun::isModelLoaded() const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		return !loadedModels_.empty();
	}

	std::string ShapeModeManagerBun::currentModelId() const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		return loadedModels_.empty() ? std::string{} : loadedModels_.front().modelId;
	}

	std::vector<std::string> ShapeModeManagerBun::getLoadedModelIds() const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		std::vector<std::string> ids;
		ids.reserve(loadedModels_.size());
		for (const auto& m : loadedModels_)
			ids.push_back(m.modelId);
		return ids;
	}

	int ShapeModeManagerBun::getLoadedModelCount() const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		return static_cast<int>(loadedModels_.size());
	}

	// ===== 多模型匹配 =====

	std::vector<MatchResult> ShapeModeManagerBun::match(const HalconCpp::HImage& image)
	{
		using namespace HalconCpp;
		std::vector<MatchResult> results;

		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		if (loadedModels_.empty())
			return results;
		if (!image.IsInitialized())
			return results;

		// TODO(多模型阈值): minScore 目前硬编码 0.5，后续应支持每个模型独立的匹配阈值
		constexpr double kMinScore = 0.5;
		constexpr int kNumMatches = 1;
		constexpr double kMaxOverlap = 0.5;
		constexpr double kGreediness = 0.9;
		const std::string kSubPixel = "least_squares";

		for (const auto& model : loadedModels_)
		{
			if (model.handle.Length() == 0)
				continue;

			try
			{
				const double angleStart = model.data.angleStart;
				const double angleExtent = model.data.angleExtent;
				const int numLevels = model.data.numLevels;

				HTuple row, column, angle, score;
				FindShapeModel(image,
					model.handle,
					HTuple(angleStart),
					HTuple(angleExtent),
					kMinScore,
					kNumMatches,
					kMaxOverlap,
					kSubPixel.c_str(),
					(numLevels > 0 ? HTuple(numLevels) : HTuple("auto")),
					kGreediness,
					&row, &column, &angle, &score);

				if (score.Length() > 0 && score[0].D() >= kMinScore)
				{
					MatchResult r;
					r.modelId = model.modelId;
					r.modelName = model.modelName;
					r.found = true;
					r.row = row[0].D();
					r.column = column[0].D();
					r.angle = angle[0].D();
					r.score = score[0].D();
					r.offsetX = r.column - model.data.centerX;
					r.offsetY = r.row - model.data.centerY;
					results.push_back(r);
				}
			}
			catch (...)
			{
				// 单个模型匹配失败 → 跳过，继续下一个
			}
		}
		return results;
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
		unloadAllModels();
	}

	void ShapeModeManagerBun::start()
	{
	}

	void ShapeModeManagerBun::stop()
	{
	}
}
