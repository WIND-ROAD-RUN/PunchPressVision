#include "Business/StampPatternBun/StampPatternBun.hpp"

#include <cmath>
#include <iostream>

namespace
{
	// 单通道 alpha 混合：out = alpha * overlay + (1 - alpha) * base。
	// alpha 为 real 类型（0..1）；overlay/base 为 byte，最终转回 byte 输出。
	void blendChannel(const HalconCpp::HImage& overlay, const HalconCpp::HImage& base,
		const HalconCpp::HImage& alpha, HalconCpp::HImage* out)
	{
		HalconCpp::HImage o, b, a;
		HalconCpp::ConvertImageType(overlay, &o, "real");
		HalconCpp::ConvertImageType(base, &b, "real");
		HalconCpp::ConvertImageType(alpha, &a, "real");

		// 1 - alpha
		HalconCpp::HImage invA;
		HalconCpp::ScaleImage(a, &invA, -1.0, 1.0);

		HalconCpp::HImage t1, t2, sum;
		HalconCpp::MultImage(o, a, &t1, 1.0, 0.0);      // overlay * alpha
		HalconCpp::MultImage(b, invA, &t2, 1.0, 0.0);   // base * (1 - alpha)
		HalconCpp::AddImage(t1, t2, &sum, 1.0, 0.0);    // 相加

		HalconCpp::ConvertImageType(sum, out, "byte");
	}
}

namespace bun
{
	StampPatternBun::StampPatternBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	// ---------- 图库 CRUD ----------

	Config::StampPatternInfo StampPatternBun::importPattern(const std::string& sourceImagePath, const std::string& name,
		bool fromDxf)
	{
		Config::StampPatternInfo info = inf_.stamp_pattern_module_->importStampPattern(sourceImagePath, name, fromDxf);
		if (!info.getId().empty())
			emit patternListChanged();
		return info;
	}

	void StampPatternBun::deletePattern(const std::string& id)
	{
		// 若已加载，先卸载，避免残留已删除套版的内存副本
		unloadPattern(id);
		inf_.stamp_pattern_module_->deleteStampPattern(id);
		emit patternListChanged();
	}

	void StampPatternBun::renamePattern(const std::string& id, const std::string& newName)
	{
		inf_.stamp_pattern_module_->changeStampPattern(id, newName);
		emit patternListChanged();
	}

	void StampPatternBun::updatePatternData(const std::string& id, const Config::StampPatternData& data)
	{
		inf_.stamp_pattern_module_->changeStampPattern(id, data);

		// 同步已加载内存副本：图片保持不变，仅更新对齐/透明度
		{
			std::unique_lock<std::shared_mutex> lock(patternMutex_);
			auto it = loadedPatterns_.find(id);
			if (it != loadedPatterns_.end())
			{
				it->second.alignRow = data.alignRow;
				it->second.alignCol = data.alignCol;
				it->second.alignAngle = data.alignAngle;
				it->second.alignScale = data.alignScale;
				it->second.alpha = data.alpha;
			}
		}
		emit patternListChanged();
	}

	// ---------- 查询 ----------

	std::vector<Config::StampPatternInfo> StampPatternBun::getAllPatterns() const
	{
		return inf_.stamp_pattern_module_->getStampPatternInfos();
	}

	Config::StampPatternItem StampPatternBun::getPatternItem(const std::string& id) const
	{
		return inf_.stamp_pattern_module_->getStampPatternItem(id);
	}

	// ---------- 加载 / 卸载 ----------

	bool StampPatternBun::loadPattern(const std::string& id)
	{
		Config::StampPatternItem item = inf_.stamp_pattern_module_->getStampPatternItem(id);
		if (item.info.getId().empty() || !item.data._patternImage.IsInitialized())
			return false;

		{
			std::unique_lock<std::shared_mutex> lock(patternMutex_);
			loadedPatterns_[id] = item.data;
		}
		emit patternLoaded(QString::fromStdString(id));
		return true;
	}

	bool StampPatternBun::unloadPattern(const std::string& id)
	{
		bool removed = false;
		{
			std::unique_lock<std::shared_mutex> lock(patternMutex_);
			removed = (loadedPatterns_.erase(id) > 0);
		}
		if (removed)
			emit patternUnloaded(QString::fromStdString(id));
		return removed;
	}

	void StampPatternBun::unloadAllPatterns()
	{
		{
			std::unique_lock<std::shared_mutex> lock(patternMutex_);
			loadedPatterns_.clear();
		}
		emit patternsUnloaded();
	}

	bool StampPatternBun::isPatternLoaded(const std::string& id) const
	{
		std::shared_lock<std::shared_mutex> lock(patternMutex_);
		return loadedPatterns_.find(id) != loadedPatterns_.end();
	}

	std::vector<std::string> StampPatternBun::getLoadedPatternIds() const
	{
		std::shared_lock<std::shared_mutex> lock(patternMutex_);
		std::vector<std::string> ids;
		ids.reserve(loadedPatterns_.size());
		for (const auto& kv : loadedPatterns_)
			ids.push_back(kv.first);
		return ids;
	}

	int StampPatternBun::getLoadedPatternCount() const
	{
		std::shared_lock<std::shared_mutex> lock(patternMutex_);
		return static_cast<int>(loadedPatterns_.size());
	}

	bool StampPatternBun::getLoadedPatternData(const std::string& id, Config::StampPatternData& out) const
	{
		std::shared_lock<std::shared_mutex> lock(patternMutex_);
		auto it = loadedPatterns_.find(id);
		if (it == loadedPatterns_.end())
			return false;
		out = it->second;
		return true;
	}

	bool StampPatternBun::getPatternData(const std::string& id, Config::StampPatternData& out)
	{
		if (getLoadedPatternData(id, out))
			return true;
		if (!loadPattern(id))
			return false;
		return getLoadedPatternData(id, out);
	}

	// ---------- 对齐变换与叠加合成 ----------

	HalconCpp::HTuple StampPatternBun::buildAlignHomMat2D(const Config::StampPatternData& data)
	{
		HalconCpp::HTuple h;
		HalconCpp::HomMat2dIdentity(&h);
		HalconCpp::HomMat2dScale(h, data.alignScale, data.alignScale, 0.0, 0.0, &h);
		HalconCpp::HomMat2dRotate(h, data.alignAngle, 0.0, 0.0, &h);
		HalconCpp::HomMat2dTranslate(h, data.alignRow, data.alignCol, &h);
		return h;
	}

	double StampPatternBun::pixelsPerWorldUnit(inf::infrastructure& inf)
	{
		using namespace HalconCpp;
		try
		{
			if (!inf.nine_point_module_)
				return 1.0;
			const auto& cfg = inf.nine_point_module_->ninePointConfig;
			if (cfg.outHomMat2D.TupleLength() < 6)
				return 1.0;

			// outHomMat2D 为 像素 -> 世界(mm)；取其逆变换，测量世界单位向量对应的像素长度
			HTuple invH;
			HomMat2dInvert(cfg.outHomMat2D, &invH);

			HTuple r0, c0, r1, c1, r2, c2;
			AffineTransPoint2d(invH, 0.0, 0.0, &r0, &c0);
			AffineTransPoint2d(invH, 1.0, 0.0, &r1, &c1);   // 世界 row 方向 1mm
			AffineTransPoint2d(invH, 0.0, 1.0, &r2, &c2);   // 世界 col 方向 1mm

			const double lenRow = std::hypot(r1[0].D() - r0[0].D(), c1[0].D() - c0[0].D());
			const double lenCol = std::hypot(r2[0].D() - r0[0].D(), c2[0].D() - c0[0].D());
			const double k = (lenRow + lenCol) / 2.0;
			return (k > 1e-12) ? k : 1.0;
		}
		catch (...)
		{
			return 1.0;
		}
	}

	HalconCpp::HImage StampPatternBun::compositeOverlay(const HalconCpp::HImage& base,
		const HalconCpp::HImage& patternRGBA, const HalconCpp::HTuple& H_pat2base, int alpha)
	{
		HalconCpp::HImage result = base;
		try
		{
			// HomMat2D 为 6 元组（仿射 2x3，末行 [0,0,1] 隐式）
			if (!patternRGBA.IsInitialized() || H_pat2base.TupleLength() != 6)
				return result;

			// 套版图需为 RGBA（4 通道）
			if (patternRGBA.CountChannels().I() != 4)
				return result;

			HalconCpp::HImage R, G, B, A;
			HalconCpp::Decompose4(patternRGBA, &R, &G, &B, &A);

			// 基图统一为 3 通道 RGB
			HalconCpp::HImage baseRgb;
			if (base.CountChannels().I() == 1)
				HalconCpp::Compose3(base, base, base, &baseRgb);
			else
				baseRgb = base;

			// affine_trans_image_size 使用"输出 -> 输入"的逆变换
			HalconCpp::HTuple H_base2pat;
			HalconCpp::HomMat2dInvert(H_pat2base, &H_base2pat);

			const int w = baseRgb.Width().I();
			const int h = baseRgb.Height().I();

			HalconCpp::HImage Rt, Gt, Bt, At;
			HalconCpp::AffineTransImageSize(R, &Rt, H_base2pat, "constant", w, h);
			HalconCpp::AffineTransImageSize(G, &Gt, H_base2pat, "constant", w, h);
			HalconCpp::AffineTransImageSize(B, &Bt, H_base2pat, "constant", w, h);
			HalconCpp::AffineTransImageSize(A, &At, H_base2pat, "constant", w, h);

			// 归一化 alpha（real 类型）：图像自身 alpha 通道 × 整体透明度 → [0,1]
			// （与 ShapeEditor::compositeStamp 保持一致，修改时需同步）
			HalconCpp::HImage alphaN;
			const double globalAlpha = static_cast<double>(alpha) / 255.0;
			HalconCpp::ConvertImageType(At, &alphaN, "real");
			HalconCpp::ScaleImage(alphaN, &alphaN, globalAlpha / 255.0, 0.0);

			HalconCpp::HImage baseR, baseG, baseB;
			HalconCpp::Decompose3(baseRgb, &baseR, &baseG, &baseB);

			HalconCpp::HImage outR, outG, outB;
			blendChannel(Rt, baseR, alphaN, &outR);
			blendChannel(Gt, baseG, alphaN, &outG);
			blendChannel(Bt, baseB, alphaN, &outB);

			HalconCpp::Compose3(outR, outG, outB, &result);
		}
		catch (const HalconCpp::HException& e)
		{
			// 合成失败返回基图，保证叠加显示不影响主流程
			std::cerr << "[StampPatternBun] compositeOverlay 异常: " << e.ErrorMessage().Text() << std::endl;
			result = base;
		}
		catch (...)
		{
			result = base;
		}
		return result;
	}

	void StampPatternBun::build()
	{
		// 无独立资源，加载态由 UI 显式触发
	}

	void StampPatternBun::destroy()
	{
		unloadAllPatterns();
	}

	void StampPatternBun::start()
	{
	}

	void StampPatternBun::stop()
	{
	}
}
