#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <json/json.h>

#include "global/GlobalPath.hpp"

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
			double findcenterX = 0.0, findcenterY = 0.0;
			double centerX = 0.0, centerY = 0.0;
			if (req.roi.IsInitialized())
			{
				HTuple area, row, col;
				AreaCenter(req.roi, &area, &row, &col);
				if (area.Length() > 0 && area[0].D() > 0)
				{
					findcenterX = row[0].D();
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
				0,
				HalconCpp::HTuple(360).TupleRad(),
				0.5,    // MinScore
				1,      // NumMatches
				0.5,    // MaxOverlap
				"least_squares",
				0,                                    // NumLevels
				0.9,    // Greediness
				&matchRow, &matchCol, &matchAngle, &matchScore);

			if (matchScore.Length() == 0 || matchScore[0].D() < 0.5)
			{
				ClearShapeModel(modelID);
				if (errorMsg) *errorMsg = "模板创建后无法在图像中匹配到自身，请调整 ROI 或训练参数";
				return false;
			}

			// 找到模型中心，作为模板中心点
			findcenterX = matchCol[0].D();
			findcenterY = matchRow[0].D();

			// 使用手动指定的中心点（优先于 ROI 形心）
			if (req.hasCenterPoint)
			{
				centerX = req.centerPoint.x();
				centerY = req.centerPoint.y();
			}
			else
			{
				centerX = findcenterX;
				centerY = findcenterY;


			}
			// 4. 准备模型数据
			outData._templateMatImage = templateImage;
			outData._originalImage = req.rawImage.IsInitialized() ? req.rawImage : req.trainingImage;
			outData.hv_ModelID = modelID;
			outData._createModelExposureTime = req.exposure;
			outData._createModelGain = req.gain;
			outData.upperLight = req.upperLight;
			outData.lowerLight = req.lowerLight;
			outData.centerX = centerX;
			outData.centerY = centerY;
			outData.findCenterX = findcenterX;
			outData.findCenterY = findcenterY;

			// 图像预处理参数
			outData._createModelPreProcessType = req.imageChannelType;
			outData._createModelUseOpening = req.useOpening;
			outData._createModelOpeningRadius = req.openingSize;
			outData._createModelUseClosing = req.useClosing;
			outData._createModelClosingRadius = req.closingSize;
			outData._createModelUseMean = req.useMean;
			outData._createModelMeanRadius = req.meanSize;

			// 训练参数
			outData.angleStart = req.angleStart;
			outData.angleExtent = req.angleExtent;
			outData.contrast = req.contrast;
			outData.minContrast = req.minContrast;
			outData.minScore = req.minScore;

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
			HObject modelContours;
			GetShapeModelContours(&modelContours, modelID, 1);

			HTuple homMat2D;
			VectorAngleToRigid(0, 0, 0, matchRow[0], matchCol[0], matchAngle[0], &homMat2D);

			HObject transformedContours;
			AffineTransContourXld(modelContours, &transformedContours, homMat2D);

			outData._findCreateXldObj = transformedContours;
			emit modelContoursFound(transformedContours);

			// 6. 生成模板缩略图：在原图上叠加 ROI 区域轮廓与 ShapeModel 轮廓
			//    使用固定大小缩略图窗口，使线宽相对图像内容保持可见
			try
			{
				HImage displayImage = req.trainingImage;
				HTuple imgW, imgH;
				displayImage.GetImageSize(&imgW, &imgH);
				const int imgWi = imgW[0].I(), imgHi = imgH[0].I();
				if (imgWi > 0 && imgHi > 0)
				{
					// 缩略图最大边长（保持宽高比）
					constexpr int kMaxThumb = 800;
					int winW = imgWi, winH = imgHi;
					if (imgWi > kMaxThumb || imgHi > kMaxThumb)
					{
						const double scale = static_cast<double>(kMaxThumb) / std::max(imgWi, imgHi);
						winW = static_cast<int>(imgWi * scale);
						winH = static_cast<int>(imgHi * scale);
						if (winW < 1) winW = 1;
						if (winH < 1) winH = 1;
					}

					// 单通道转 3 通道（Halcon 显示/保存需要 RGB）
					HImage rgbImage;
					Compose3(displayImage, displayImage, displayImage, &rgbImage);

					HTuple bufWin;
					OpenWindow(0, 0, winW, winH, 0, "buffer", "", &bufWin);
					SetPart(bufWin, 0, 0, imgHi - 1, imgWi - 1);
					DispObj(rgbImage, bufWin);

					// 绘制 ROI 区域轮廓（绿色，margin 模式只绘边框）
					SetDraw(bufWin, "margin");
					SetColor(bufWin, "green");
					SetLineWidth(bufWin, 3);
					for (const auto& obj : req._paintCreateRoiList)
					{
						if (obj.IsInitialized())
							DispObj(obj, bufWin);
					}

					// 绘制 Mask 区域轮廓（蓝色）
					SetColor(bufWin, "blue");
					SetLineWidth(bufWin, 3);
					for (const auto& obj : req._paintShieldRoiList)
					{
						if (obj.IsInitialized())
							DispObj(obj, bufWin);
					}

					// 绘制 ShapeModel 轮廓（红色，填充模式）
					SetDraw(bufWin, "fill");
					SetColor(bufWin, "red");
					SetLineWidth(bufWin, 4);
					DispObj(transformedContours, bufWin);

					DumpWindowImage(&outData._templateMatImage, bufWin);
					CloseWindow(bufWin);
				}
			}
			catch (...)
			{
				// 缩略图生成失败不阻断模型创建，保留模板原图
			}

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

			// 同步已加载模型缓存中的名称
			{
				std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
				for (auto& m : loadedModels_)
				{
					if (m.modelId == id)
					{
						m.modelName = baseInfo.name;
						break;
					}
				}
			}

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

		{
			std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);

			for (const auto& id : ids)
			{
				// 跳过已加载的模型
				if (std::any_of(loadedModels_.begin(), loadedModels_.end(),
					[&id](const LoadedModel& m) { return m.modelId == id; }))
				{
					++successCount;
					continue;
				}

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

		// 发出信号（包含全部已加载模型名称）
		{
			QStringList allNames;
			for (const auto& m : loadedModels_)
				allNames.append(QString::fromStdString(m.modelName));
			if (!allNames.isEmpty())
				emit modelLoaded(allNames.first());
			emit modelsLoaded(allNames, static_cast<int>(loadedModels_.size()), failCount);
		}

		// 持久化本次加载的模型 ID 列表
		saveLastLoadedModels();

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

	bool ShapeModeManagerBun::unloadModel(const std::string& id)
	{
		{
			std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
			const auto it = std::find_if(loadedModels_.begin(), loadedModels_.end(),
				[&id](const LoadedModel& m) { return m.modelId == id; });
			if (it == loadedModels_.end())
				return false;
			loadedModels_.erase(it);  // HTuple 析构自动释放 Halcon 句柄
		}

		if (loadedModels_.empty())
			emit modelsUnloaded();
		else
			emit modelListChanged();  // 触发 UI 刷新

		saveLastLoadedModels();
		return true;
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

	// ===== 模型偏移量 =====

	ModelUserOffset ShapeModeManagerBun::getUserOffset(const std::string& modelId) const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		for (const auto& m : loadedModels_)
		{
			if (m.modelId == modelId)
			{
				ModelUserOffset o;
				o.offsetX = m.data.offsetX;
				o.offsetY = m.data.offsetY;
				o.offsetAngle = m.data.offsetAngle;
				return o;
			}
		}
		return {};  // 未找到 → 默认 0
	}

	bool ShapeModeManagerBun::setUserOffset(const std::string& modelId,
		double offsetX, double offsetY, double offsetAngle,
		std::string* errorMsg)
	{
		try
		{
			{
				std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
				for (auto& m : loadedModels_)
				{
					if (m.modelId == modelId)
					{
						m.data.offsetX = offsetX;
						m.data.offsetY = offsetY;
						m.data.offsetAngle = offsetAngle;
						break;
					}
				}
			}

			// 持久化到磁盘（model_params.txt）
			// 需要从 infra 重新加载完整 item，替换 data 后写回
			auto item = inf_.shape_model_manager_module_->getShapeModelItem(modelId);
			item.data.offsetX = offsetX;
			item.data.offsetY = offsetY;
			item.data.offsetAngle = offsetAngle;
			inf_.shape_model_manager_module_->changeShapeModelItem(modelId, item.data);

			emit modelOffsetChanged(QString::fromStdString(modelId));
			return true;
		}
		catch (const std::exception& e)
		{
			if (errorMsg) *errorMsg = e.what();
			return false;
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "保存偏移量失败";
			return false;
		}
	}

	// ===== 模板匹配参数 =====

	ModelMatchParams ShapeModeManagerBun::getMatchParams(const std::string& modelId) const
	{
		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
		for (const auto& m : loadedModels_)
		{
			if (m.modelId == modelId)
			{
				ModelMatchParams p;
				p.numMatches = m.data.findnumber > 0 ? m.data.findnumber : 1;
				p.minScore = m.data.minScore;
				p.angleStart = m.data.angleStart;
				p.angleExtent = m.data.angleExtent;
				return p;
			}
		}
		return {};  // 未找到 → 默认值
	}

	bool ShapeModeManagerBun::setMatchParams(const std::string& modelId,
		int numMatches, double minScore, double angleStart, double angleExtent,
		std::string* errorMsg)
	{
		try
		{
			{
				std::unique_lock<std::shared_mutex> lk(modelCacheMutex_);
				for (auto& m : loadedModels_)
				{
					if (m.modelId == modelId)
					{
						m.data.findnumber = numMatches;
						m.data.minScore = minScore;
						m.data.angleStart = angleStart;
						m.data.angleExtent = angleExtent;
						break;
					}
				}
			}

			// 持久化到磁盘（model_params.txt）
			auto item = inf_.shape_model_manager_module_->getShapeModelItem(modelId);
			item.data.findnumber = numMatches;
			item.data.minScore = minScore;
			item.data.angleStart = angleStart;
			item.data.angleExtent = angleExtent;
			inf_.shape_model_manager_module_->changeShapeModelItem(modelId, item.data);

			emit matchParamsChanged(QString::fromStdString(modelId));
			return true;
		}
		catch (const std::exception& e)
		{
			if (errorMsg) *errorMsg = e.what();
			return false;
		}
		catch (...)
		{
			if (errorMsg) *errorMsg = "保存匹配参数失败";
			return false;
		}
	}

	// ===== 多模型匹配 =====

	HalconCpp::HImage ShapeModeManagerBun::preprocessImage(const HalconCpp::HImage& image,
		const Config::ShapeModelData& data) const
	{
		using namespace HalconCpp;
		if (!image.IsInitialized())
			return image;

		try
		{
			HImage result = image;

			// 1. 通道提取（与创建模板时一致）
			HTuple channels;
			CountChannels(image, &channels);
			const int channelType = data._createModelPreProcessType;
			if (channels[0].I() >= 3)
			{
				switch (channelType)
				{
				case 0: // 灰度
					Rgb1ToGray(image, &result);
					break;
				case 1: // 红
					AccessChannel(image, &result, 1);
					break;
				case 2: // 绿
					AccessChannel(image, &result, 2);
					break;
				case 3: // 蓝
					AccessChannel(image, &result, 3);
					break;
				case 4: // H（色调）
				{
					HImage r, g, b, h, s, v;
					Decompose3(image, &r, &g, &b);
					TransFromRgb(r, g, b, &h, &s, &v, "hsv");
					result = h;
					break;
				}
				case 5: // S（饱和度）
				{
					HImage r, g, b, h, s, v;
					Decompose3(image, &r, &g, &b);
					TransFromRgb(r, g, b, &h, &s, &v, "hsv");
					result = s;
					break;
				}
				case 6: // V（明度）
				{
					HImage r, g, b, h, s, v;
					Decompose3(image, &r, &g, &b);
					TransFromRgb(r, g, b, &h, &s, &v, "hsv");
					result = v;
					break;
				}
				default:
					break;
				}
			}
			else if (channelType != 0 && channels[0].I() >= channelType)
			{
				AccessChannel(image, &result, channelType);
			}

			// 2. 开运算（灰度形态学，矩形掩膜）
			if (data._createModelUseOpening && data._createModelOpeningRadius > 0)
			{
				HImage opened;
				const int openSize = data._createModelOpeningRadius * 2 + 1;
				GrayOpeningRect(result, &opened, openSize, openSize);
				result = opened;
			}

			// 3. 闭运算（灰度形态学，矩形掩膜）
			if (data._createModelUseClosing && data._createModelClosingRadius > 0)
			{
				HImage closed;
				const int closeSize = data._createModelClosingRadius * 2 + 1;
				GrayClosingRect(result, &closed, closeSize, closeSize);
				result = closed;
			}

			// 4. 均值滤波
			if (data._createModelUseMean && data._createModelMeanRadius > 0)
			{
				HImage smoothed;
				const int size = data._createModelMeanRadius * 2 + 1;
				MeanImage(result, &smoothed, size, size);
				result = smoothed;
			}

			return result;
		}
		catch (const HException&)
		{
			// 预处理失败时返回原图，不阻断匹配流程
			return image;
		}
	}

	std::vector<MatchResult> ShapeModeManagerBun::match(const HalconCpp::HImage& image)
	{
		std::vector<MatchResult> results;

		if (!image.IsInitialized())
			return results;

		// 获取九点标定矩阵（所有模型共用同一个标定矩阵）
		HalconCpp::HTuple homMat2D;
		bool hasNinePoint = false;
		if (inf_.nine_point_module_ && inf_tool_.nine_point_bun)
		{
			const auto& cfg = inf_.nine_point_module_->ninePointConfig;
			if (cfg.outHomMat2D.TupleLength() >= 6)
			{
				homMat2D = cfg.outHomMat2D;
				hasNinePoint = true;
			}
		}

		std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);

		for (const auto& model : loadedModels_)
		{
			try
			{
				// 对图像应用与创建模板时相同的预处理
				HalconCpp::HImage processed = preprocessImage(image, model.data);

				HalconCpp::HTuple row, column, angle, score;
				HalconCpp::FindShapeModel(
					processed,
					model.handle,
					HalconCpp::HTuple(model.data.angleStart).TupleRad(),
					HalconCpp::HTuple(model.data.angleExtent).TupleRad(),
					model.data.minScore,                                // MinScore
					std::max(1, model.data.findnumber),                 // NumMatches（兼容旧数据：0 → 1）
					0.5,                                                // MaxOverlap
					"least_squares",                                    // SubPixel
					HalconCpp::HTuple(0),                                // NumLevels (0 = 使用全部金字塔层级)
					0.9,                                                // Greediness
					&row, &column, &angle, &score);

					if (score.Length() > 0 && score[0].D() >= model.data.minScore)
					{
						const double matchRow = row[0].D();
						const double matchCol = column[0].D();
						const double matchAngle = angle[0].D();

						// 模板参考中心点（用于旋转/平移自定义点，与 findShapemodel 一致）
						const double refRow = model.data.findCenterY;
						const double refCol = model.data.findCenterX;

						// 自定义点（创建模板时画的点，像素坐标）
						double customRow = model.data.centerY;
						double customCol = model.data.centerX;

						// 偏移量单位是 mm，这里先换算成像素偏移量，再加到自定义点上
						// （与 findShapemodel 一致：mm→pixel 后在旋转变换前叠加，使偏移随工件旋转）
						if (hasNinePoint
							&& (std::abs(model.data.offsetX) > 1e-9 || std::abs(model.data.offsetY) > 1e-9))
						{
							try
							{
								HalconCpp::HTuple invHomMat2D;
								HalconCpp::HomMat2dInvert(homMat2D, &invHomMat2D);

								HalconCpp::HTuple basePixRow, basePixCol;
								HalconCpp::HTuple offsetPixRow, offsetPixCol;

								HalconCpp::AffineTransPoint2d(invHomMat2D, 0.0, 0.0,
									&basePixRow, &basePixCol);
								HalconCpp::AffineTransPoint2d(invHomMat2D,
									model.data.offsetX, model.data.offsetY,
									&offsetPixRow, &offsetPixCol);

								if (basePixRow.TupleLength() > 0 && basePixCol.TupleLength() > 0 &&
									offsetPixRow.TupleLength() > 0 && offsetPixCol.TupleLength() > 0)
								{
									const double deltaRow = offsetPixRow[0].D() - basePixRow[0].D();
									const double deltaCol = offsetPixCol[0].D() - basePixCol[0].D();
									customRow += deltaRow;
									customCol += deltaCol;
								}
							}
							catch (...)
							{
								// fallback: 使用原始 customRow/customCol
							}
						}

						const bool canTransform =
							(std::abs(refCol) > 1e-9) || (std::abs(refRow) > 1e-9) ||
							(std::abs(customCol) > 1e-9) || (std::abs(customRow) > 1e-9);

						// 将自定义点从模板参考系变换到当前匹配位置（与 findShapemodel 一致）
						double outRow = matchRow, outCol = matchCol;
						if (canTransform)
						{
							try
							{
								HalconCpp::HTuple h;
								HalconCpp::VectorAngleToRigid(
									refRow, refCol, 0.0,
									matchRow, matchCol, matchAngle,
									&h);

								HalconCpp::HTuple r, c;
								HalconCpp::AffineTransPoint2d(h, customRow, customCol, &r, &c);

								if (r.TupleLength() > 0 && c.TupleLength() > 0)
								{
									outRow = r[0].D();
									outCol = c[0].D();
								}
							}
							catch (...)
							{
								// fallback: 使用匹配中心
							}
						}

						MatchResult result;
						result.found = true;
						result.modelId = model.modelId;
						result.modelName = model.modelName;
						// 自定义中心点的像素坐标（供上层绘图）
						result.row = outRow;
						result.column = outCol;
						result.score = score[0].D();

						// 像素 → 世界坐标（九点标定）
						double worldX = outCol, worldY = outRow;
						if (hasNinePoint)
						{
							if (!inf_tool_.nine_point_bun->pixToWorld(homMat2D,
								outCol, outRow, worldX, worldY))
							{
								// 标定转换失败，回退到像素坐标
								worldX = outCol;
								worldY = outRow;
							}
						}
						result.realX = worldX;
						result.realY = worldY;

						// 偏移量已在像素域叠加到 customRow/customCol 中，realX/Y 已包含偏移
						result.offsetX = result.realX;
						result.offsetY = result.realY;

						// 角度补偿（matchAngle 为 Halcon 弧度制，统一转为角度制存储）
						constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
						result.angle = matchAngle * kRadToDeg + model.data.offsetAngle;

						// 生成匹配轮廓 XLD（与 findShapemodel 一致），供主界面叠加显示
						try
						{
							HalconCpp::HObject modelContours;
							HalconCpp::GetShapeModelContours(&modelContours, model.handle, 1);

							HalconCpp::HTuple hv_HomMat2D;
							HalconCpp::HomMat2dIdentity(&hv_HomMat2D);
							HalconCpp::HomMat2dRotate(hv_HomMat2D, matchAngle, 0.0, 0.0, &hv_HomMat2D);
							HalconCpp::HomMat2dTranslate(hv_HomMat2D, matchRow, matchCol, &hv_HomMat2D);

							HalconCpp::HXLDCont ho_TransContours;
							HalconCpp::AffineTransContourXld(modelContours, &ho_TransContours, hv_HomMat2D);

							// 叠加自定义中心点十字
							HalconCpp::HXLDCont ho_Cross;
							HalconCpp::GenCrossContourXld(&ho_Cross, outRow, outCol, 24.0, matchAngle);
							HalconCpp::ConcatObj(ho_TransContours, ho_Cross, &result.matchedContours);
						}
						catch (...)
						{
							// 轮廓生成失败不影响匹配结果
						}

						results.push_back(result);
					}
			}
			catch (const HalconCpp::HException&)
			{
				// 单个模型匹配失败不中断其余模型的匹配
				continue;
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
				"auto",                                     // NumLevels
				HTuple(req.angleStart).TupleRad(),
				HTuple(req.angleExtent).TupleRad(),
				"auto",                                     // AngleStep
				"auto",                                     // Optimization
				"use_polarity",                             // Metric
				req.contrast, req.minContrast,
				&modelID);

			// 3. 匹配测试
			HTuple row, column, angle, score;
			FindShapeModel(req.trainingImage, modelID,
				HTuple(req.angleStart).TupleRad(), HTuple(req.angleExtent).TupleRad(),
				0.3, 1, 0.5, "least_squares", 0, 0.9,
				&row, &column, &angle, &score);

			if (score.Length() > 0 && score[0].D() >= 0.3)
			{
				result.found = true;
				result.row = row[0].D();
				result.column = column[0].D();
				result.angle = angle[0].D() * 180.0 / 3.14159265358979323846;
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
		// 自动恢复上次加载的模型集合
		loadLastLoadedModels();
	}

	void ShapeModeManagerBun::stop()
	{
	}

	// ===== 持久化：上次加载的模型 ID =====

	void ShapeModeManagerBun::saveLastLoadedModels()
	{
		try
		{
			const std::string configDir = global::path::configDir();
			std::filesystem::create_directories(configDir);
			const std::string filePath = configDir + "last_loaded_models.json";

			Json::Value root(Json::arrayValue);
			{
				std::shared_lock<std::shared_mutex> lk(modelCacheMutex_);
				for (const auto& m : loadedModels_)
					root.append(m.modelId);
			}

			// 原子写入：先写 .tmp，再重命名
			const std::string tmpPath = filePath + ".tmp";
			{
				std::ofstream ofs(tmpPath, std::ios::out | std::ios::trunc);
				if (!ofs)
					return;
				Json::StreamWriterBuilder builder;
				builder["indentation"] = "  ";
				std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
				writer->write(root, &ofs);
				ofs << '\n';
			}
			std::filesystem::rename(tmpPath, filePath);
		}
		catch (...)
		{
			// 持久化失败不应影响正常流程
		}
	}

	void ShapeModeManagerBun::loadLastLoadedModels()
	{
		try
		{
			const std::string filePath = global::path::configDir() + "last_loaded_models.json";
			if (!std::filesystem::exists(filePath))
				return;

			Json::Value root;
			{
				std::ifstream ifs(filePath);
				if (!ifs)
					return;
				Json::CharReaderBuilder builder;
				builder["collectComments"] = false;
				std::string errs;
				if (!Json::parseFromStream(builder, ifs, &root, &errs))
					return;
			}

			if (!root.isArray() || root.empty())
				return;

			std::vector<std::string> ids;
			for (const auto& val : root)
			{
				if (val.isString())
					ids.push_back(val.asString());
			}

			if (!ids.empty())
				loadModels(ids);
		}
		catch (...)
		{
			// 恢复失败不阻塞启动
		}
	}
}
