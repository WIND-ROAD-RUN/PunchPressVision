#include "Business/StampPatternBun/StampPatternBun.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

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

	// 提取图像某通道的矩形区域为原点 (0,0) 的局部坐标 byte 图像。
	// crop_rectangle1 会保留绝对原点，而 MultImage/AddImage 按绝对坐标系求
	// domain 交集，与 ROI 局部仿射结果（原点 (0,0)）直接运算会交集为空
	// （实测验证），故用指针逐行拷贝出局部图像。
	HalconCpp::HImage extractChannelRoiLocal(const HalconCpp::HImage& img, int channel,
		int row1, int col1, int row2, int col2)
	{
		const int roiW = col2 - col1 + 1;
		const int roiH = row2 - row1 + 1;

		HalconCpp::HImage ch = img.AccessChannel(channel);
		HalconCpp::HRegion rect;
		HalconCpp::GenRectangle1(&rect, row1, col1, row2, col2);
		HalconCpp::HImage reduced = ch.ReduceDomain(rect);

		Hlong w = 0, h = 0, vPitch = 0, hBitPitch = 0, bpp = 0;
		const unsigned char* src = static_cast<const unsigned char*>(
			reduced.GetImagePointer1Rect(&w, &h, &vPitch, &hBitPitch, &bpp));
		if (!src || bpp != 8 || w != roiW || h != roiH)
			throw HalconCpp::HException("extractChannelRoiLocal", "非 byte 图像或区域尺寸不符");

		std::vector<unsigned char> buf(static_cast<size_t>(roiW) * roiH);
		for (int r = 0; r < roiH; ++r)
			std::memcpy(buf.data() + static_cast<size_t>(r) * roiW,
				src + static_cast<size_t>(r) * vPitch, roiW);

		HalconCpp::HImage local;
		local.GenImage1("byte", roiW, roiH, buf.data());
		return local;
	}

	// 把 ROI 局部坐标的 3 通道 byte 图像逐行写入三份平面缓冲 (row1, col1) 处
	void pasteRoiIntoPlanes(unsigned char* dst[3], size_t dstWidth,
		const HalconCpp::HImage& roiRgb, int row1, int col1, int roiW, int roiH)
	{
		for (int c = 0; c < 3; ++c)
		{
			HalconCpp::HImage ch = roiRgb.AccessChannel(c + 1);
			Hlong w = 0, h = 0;
			HalconCpp::HString t;
			const unsigned char* src = static_cast<const unsigned char*>(
				ch.GetImagePointer1(&t, &w, &h));
			if (!src || t != "byte" || w != roiW || h != roiH)
				throw HalconCpp::HException("pasteRoiIntoPlanes", "ROI 通道尺寸不符");
			for (int r = 0; r < roiH; ++r)
				std::memcpy(dst[c] + (static_cast<size_t>(row1 + r)) * dstWidth + col1,
					src + static_cast<size_t>(r) * roiW, roiW);
		}
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

			// 全透明时无需合成
			if (alpha <= 0)
				return result;

			const int imgW = base.Width().I();
			const int imgH = base.Height().I();
			const int patW = patternRGBA.Width().I();
			const int patH = patternRGBA.Height().I();

			// ---------- ROI：套版四角变换后的包围盒（外扩 2px 供插值） ----------
			// 整图仿射 + 整图混合的开销是 O(整图)，生产帧率下不可接受；
			// 套版只有 ~200x300 像素，只在包围盒内合成再贴回原图。
			const double cr[4] = { 0.0, 0.0, patH - 1.0, patH - 1.0 };
			const double cc[4] = { 0.0, patW - 1.0, 0.0, patW - 1.0 };
			const HalconCpp::HTuple cornerR(cr, 4);
			const HalconCpp::HTuple cornerC(cc, 4);
			HalconCpp::HTuple tfR, tfC;
			HalconCpp::AffineTransPixel(H_pat2base, cornerR, cornerC, &tfR, &tfC);

			const double rMin = tfR.TupleMin().D();
			const double rMax = tfR.TupleMax().D();
			const double cMin = tfC.TupleMin().D();
			const double cMax = tfC.TupleMax().D();

			const int row1 = std::max(0, static_cast<int>(std::floor(rMin)) - 2);
			const int col1 = std::max(0, static_cast<int>(std::floor(cMin)) - 2);
			const int row2 = std::min(imgH - 1, static_cast<int>(std::ceil(rMax)) + 2);
			const int col2 = std::min(imgW - 1, static_cast<int>(std::ceil(cMax)) + 2);
			if (row1 > row2 || col1 > col2)
				return result;   // 套版完全落在图像外

			const int roiW = col2 - col1 + 1;
			const int roiH = row2 - row1 + 1;

			// 底图 ROI 提取为局部坐标通道（原点 (0,0)，与下方仿射输出对齐）
			HalconCpp::HImage baseR, baseG, baseB;
			const int baseChannels = base.CountChannels().I();
			if (baseChannels == 1)
			{
				baseR = extractChannelRoiLocal(base, 1, row1, col1, row2, col2);
				baseG = baseR;
				baseB = baseR;
			}
			else
			{
				baseR = extractChannelRoiLocal(base, 1, row1, col1, row2, col2);
				baseG = extractChannelRoiLocal(base, 2, row1, col1, row2, col2);
				baseB = extractChannelRoiLocal(base, 3, row1, col1, row2, col2);
			}

			// affine_trans_image_size 的 HomMat2D 为"输入 -> 输出"变换，
			// 直接传入即可（输出 domain = H·输入 domain，传逆矩阵会把
			// domain 映到图像外导致全黑——实测验证）。
			// 输出为 ROI 局部坐标：在 H 之后再左乘一个平移 T(-row1, -col1)
			HalconCpp::HTuple H_roi;
			HalconCpp::HomMat2dTranslate(H_pat2base,
				-static_cast<double>(row1), -static_cast<double>(col1), &H_roi);

			HalconCpp::HImage R, G, B, A;
			HalconCpp::Decompose4(patternRGBA, &R, &G, &B, &A);

			HalconCpp::HImage Rt, Gt, Bt, At;
			HalconCpp::AffineTransImageSize(R, &Rt, H_roi, "constant", roiW, roiH);
			HalconCpp::AffineTransImageSize(G, &Gt, H_roi, "constant", roiW, roiH);
			HalconCpp::AffineTransImageSize(B, &Bt, H_roi, "constant", roiW, roiH);
			HalconCpp::AffineTransImageSize(A, &At, H_roi, "constant", roiW, roiH);

			// 变换输出的 domain 只有套版包围盒大小，而 MultImage/AddImage 只在
			// domain 交集上计算，不扩回 ROI 全图会导致合成结果只剩套版区域。
			// domain 外灰度为 0：alpha=0 即全透明，RGB 通道会被 alpha=0 屏蔽，扩展是安全的。
			HalconCpp::FullDomain(Rt, &Rt);
			HalconCpp::FullDomain(Gt, &Gt);
			HalconCpp::FullDomain(Bt, &Bt);
			HalconCpp::FullDomain(At, &At);

			// 归一化 alpha（real 类型）：图像自身 alpha 通道 × 整体透明度 → [0,1]
			// （编辑预览已改为 DispObj 显示时绘制，不做像素合成；本函数用于
			// 需要合成结果落盘的场景）
			HalconCpp::HImage alphaN;
			const double globalAlpha = static_cast<double>(alpha) / 255.0;
			HalconCpp::ConvertImageType(At, &alphaN, "real");
			HalconCpp::ScaleImage(alphaN, &alphaN, globalAlpha / 255.0, 0.0);

			HalconCpp::HImage outR, outG, outB;
			blendChannel(Rt, baseR, alphaN, &outR);
			blendChannel(Gt, baseG, alphaN, &outG);
			blendChannel(Bt, baseB, alphaN, &outB);

			HalconCpp::HImage roiComposited;
			HalconCpp::Compose3(outR, outG, outB, &roiComposited);

			// ---------- 贴回整图 ----------
			// Compose3(base,base,base) 会让 3 个通道共享同一块像素内存
			// （实测：指针写入 R 后被 G/B 通道写覆盖成全黑），不能用指针回写。
			// 自管三份独立平面缓冲，paste 后用 GenImage1（拷贝语义）组装结果。
			const size_t planeSize = static_cast<size_t>(imgW) * imgH;
			std::vector<unsigned char> planeR, planeG, planeB;
			if (baseChannels == 1)
			{
				Hlong w = 0, h = 0;
				HalconCpp::HString t;
				const unsigned char* p = static_cast<const unsigned char*>(
					base.GetImagePointer1(&t, &w, &h));
				if (!p || t != "byte" || w != imgW || h != imgH)
					throw HalconCpp::HException("compositeOverlay", "底图非 byte");
				planeR.assign(p, p + planeSize);
				planeG = planeR;
				planeB = planeR;
			}
			else
			{
				void* r = nullptr;
				void* g = nullptr;
				void* b = nullptr;
				Hlong w = 0, h = 0;
				HalconCpp::HString t;
				base.GetImagePointer3(&r, &g, &b, &t, &w, &h);
				if (!r || !g || !b || t != "byte" || w != imgW || h != imgH)
					throw HalconCpp::HException("compositeOverlay", "底图非 3 通道 byte");
				const unsigned char* cr = static_cast<const unsigned char*>(r);
				const unsigned char* cg = static_cast<const unsigned char*>(g);
				const unsigned char* cb = static_cast<const unsigned char*>(b);
				planeR.assign(cr, cr + planeSize);
				planeG.assign(cg, cg + planeSize);
				planeB.assign(cb, cb + planeSize);
			}

			unsigned char* dst[3] = { planeR.data(), planeG.data(), planeB.data() };
			pasteRoiIntoPlanes(dst, imgW, roiComposited, row1, col1, roiW, roiH);

			HalconCpp::HImage chR, chG, chB;
			chR.GenImage1("byte", imgW, imgH, planeR.data());
			chG.GenImage1("byte", imgW, imgH, planeG.data());
			chB.GenImage1("byte", imgW, imgH, planeB.data());
			HalconCpp::Compose3(chR, chG, chB, &result);
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
