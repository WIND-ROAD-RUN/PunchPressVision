#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

namespace bun
{
	ShapeModeManagerBun::ShapeModeManagerBun(inf::infrastructure& inf)
		: inf_(inf)
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
