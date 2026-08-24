#include "infrastructure/StampPatternModule/DxfPatternRenderer.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

#include <QImage>

#include "halconcpp/HalconCpp.h"

namespace inf
{
	namespace
	{
		// Halcon 默认将新生成的区域裁剪到默认图像尺寸（512x512，
		// clip_region=true），大幅面 DXF 布局后的坐标远超该范围，
		// 会导致线区域整体为空。渲染期间临时关闭裁剪，退出时还原。
		struct ClipRegionGuard
		{
			HalconCpp::HTuple old;
			ClipRegionGuard()
			{
				try
				{
					HalconCpp::GetSystem("clip_region", &old);
					HalconCpp::SetSystem("clip_region", "false");
				}
				catch (...) {}
			}
			~ClipRegionGuard()
			{
				try
				{
					if (old.TupleLength() > 0)
						HalconCpp::SetSystem("clip_region", old);
				}
				catch (...) {}
			}
		};

		// 图纸四周留白（像素）
		constexpr double kMarginPx = 12.0;
		// 输出图片最大边长，防止超大图纸生成巨型图片
		constexpr double kMaxImageSide = 4096.0;

		// 计算全部 XLD 轮廓的整体包围盒。无轮廓时返回 false。
		bool contoursBoundingBox(const HalconCpp::HObject& contours,
			double& minRow, double& minCol, double& maxRow, double& maxCol)
		{
			HalconCpp::HTuple r1, c1, r2, c2;
			HalconCpp::SmallestRectangle1Xld(contours, &r1, &c1, &r2, &c2);
			if (r1.TupleLength() == 0)
				return false;

			minRow = r1.TupleMin().D();
			minCol = c1.TupleMin().D();
			maxRow = r2.TupleMax().D();
			maxCol = c2.TupleMax().D();
			return (maxRow > minRow) && (maxCol > minCol);
		}

		// 布局：轮廓坐标 -> 图像坐标（过大整体缩放保持长宽比 + 平移留白）。
		// 输出仿射矩阵与图像尺寸；无有效轮廓返回 false。
		bool computeLayout(const HalconCpp::HObject& contours,
			HalconCpp::HTuple& hmOut, int& wOut, int& hOut)
		{
			using namespace HalconCpp;

			double minRow, minCol, maxRow, maxCol;
			if (!contoursBoundingBox(contours, minRow, minCol, maxRow, maxCol))
				return false;

			HTuple hm;
			HomMat2dIdentity(&hm);

			const double span = (std::max)(maxRow - minRow, maxCol - minCol);
			if (span > kMaxImageSide)
			{
				const double s = kMaxImageSide / span;
				HomMat2dScale(hm, s, s, 0.0, 0.0, &hm);

				HObject scaled;
				AffineTransContourXld(contours, &scaled, hm);
				if (!contoursBoundingBox(scaled, minRow, minCol, maxRow, maxCol))
					return false;
			}

			HomMat2dTranslate(hm, kMarginPx - minRow, kMarginPx - minCol, &hm);

			wOut = static_cast<int>(maxCol - minCol + 2.0 * kMarginPx + 0.5);
			hOut = static_cast<int>(maxRow - minRow + 2.0 * kMarginPx + 0.5);
			if (wOut <= 0 || hOut <= 0)
				return false;

			hmOut = hm;
			return true;
		}

		// 按布局放置轮廓并绘制线条通道（蓝通道 + alpha 通道）。
		// 线条固定 1px（最细），不做膨胀加粗。
		void paintLineChannels(const HalconCpp::HObject& contours,
			const HalconCpp::HTuple& hm, int width, int height,
			HalconCpp::HImage& blueOut, HalconCpp::HImage& alphaOut)
		{
			using namespace HalconCpp;

			HObject placed;
			AffineTransContourXld(contours, &placed, hm);

			// gen_region_contour_xld('margin') 对“开放 + 亚像素坐标”的轮廓会
			// 产生空区域（Halcon 24.11 实测），而 DXF 读入的多段线大多如此；
			// paint_xld 又会填充闭合区域。因此逐线段 gen_region_line 栅格化，
			// 对开放/闭合/亚像素轮廓均可靠。
			std::vector<double> br, bc, er, ec;
			HTuple objCount;
			CountObj(placed, &objCount);
			for (Hlong i = 1; i <= objCount.I(); ++i)
			{
				HObject one;
				SelectObj(placed, &one, i);
				HTuple rows, cols;
				GetContourXld(one, &rows, &cols);
				const Hlong m = rows.TupleLength();
				for (Hlong j = 0; j + 1 < m; ++j)
				{
					const double r0 = rows[j].D(), c0 = cols[j].D();
					const double r1 = rows[j + 1].D(), c1 = cols[j + 1].D();
					if (r0 == r1 && c0 == c1)
						continue; // 零长线段（DXF 垃圾点）
					br.push_back(r0); bc.push_back(c0);
					er.push_back(r1); ec.push_back(c1);
				}
			}

			HObject unionObj;
			if (!br.empty())
			{
				HObject lines;
				GenRegionLine(&lines,
					HTuple(br.data(), static_cast<Hlong>(br.size())),
					HTuple(bc.data(), static_cast<Hlong>(bc.size())),
					HTuple(er.data(), static_cast<Hlong>(er.size())),
					HTuple(ec.data(), static_cast<Hlong>(ec.size())));
				Union1(lines, &unionObj);
			}
			else
			{
				GenEmptyRegion(&unionObj);
			}
			HRegion lineRegion(unionObj);

			GenImageConst(&blueOut, "byte", width, height);
			PaintRegion(lineRegion, blueOut, &blueOut, 255, "fill");

			GenImageConst(&alphaOut, "byte", width, height);
			PaintRegion(lineRegion, alphaOut, &alphaOut, 255, "fill");
		}
	}

	bool loadDxfContours(const std::string& dxfPath, HalconCpp::HObject& contoursOut)
	{
		using namespace HalconCpp;
		try
		{
			HObject contours;
			HTuple dxfStatus;
			ReadContourXldDxf(&contours, dxfPath.c_str(), HTuple(), HTuple(), &dxfStatus);

			HTuple objCount;
			CountObj(contours, &objCount);
			if (objCount.I() <= 0)
				return false;

			// 垂直镜像：DXF 的 y 直接映射为 row，而图像 row 轴向下，
			// 需翻转行坐标才能与 CAD 看图方向一致。
			// 注意 Halcon 仿射点约定 (row, col)：HomMat2dScale 的 Sx 作用于
			// 第一个元素（row），因此翻行用 Sx=-1（实测验证）。
			HTuple hm;
			HomMat2dIdentity(&hm);
			HomMat2dScale(hm, -1.0, 1.0, 0.0, 0.0, &hm);
			HObject oriented;
			AffineTransContourXld(contours, &oriented, hm);

			contoursOut = oriented;
			return true;
		}
		catch (const HException& e)
		{
			std::cerr << "[loadDxfContours] Halcon异常: " << e.ErrorMessage().Text() << std::endl;
			return false;
		}
		catch (...)
		{
			return false;
		}
	}

	void filterDxfContoursByRegions(const HalconCpp::HObject& contours,
		const std::vector<DxfRect>& rects, HalconCpp::HObject& selectedOut)
	{
		using namespace HalconCpp;

		if (rects.empty())
		{
			selectedOut = contours;
			return;
		}

		HObject result;
		GenEmptyObj(&result);
		try
		{
			// 用各轮廓包围盒中心判定归属（对零长度的点轮廓同样有效）
			HTuple r1, c1, r2, c2;
			SmallestRectangle1Xld(contours, &r1, &c1, &r2, &c2);
			const Hlong n = r1.TupleLength();

			for (Hlong i = 0; i < n; ++i)
			{
				const double cr = (r1[i].D() + r2[i].D()) / 2.0;
				const double cc = (c1[i].D() + c2[i].D()) / 2.0;

				bool inside = false;
				for (const auto& rc : rects)
				{
					if (cr >= rc.row1 && cr <= rc.row2 && cc >= rc.col1 && cc <= rc.col2)
					{
						inside = true;
						break;
					}
				}
				if (!inside)
					continue;

				HObject one, merged;
				SelectObj(contours, &one, i + 1);
				ConcatObj(result, one, &merged);
				result = merged;
			}
		}
		catch (const HException& e)
		{
			std::cerr << "[filterDxfContoursByRegions] Halcon异常: " << e.ErrorMessage().Text() << std::endl;
		}
		selectedOut = result;
	}

	bool renderDxfContoursToRgbaPng(const HalconCpp::HObject& contours, const std::string& pngPath)
	{
		using namespace HalconCpp;
		try
		{
			ClipRegionGuard clipGuard;

			HTuple hm;
			int width, height;
			if (!computeLayout(contours, hm, width, height))
				return false;

			// 线条通道：蓝色（B=255）；背景黑色（RGB=0，alpha=0 保持叠加时透明）
			HImage blueChan, alphaChan;
			paintLineChannels(contours, hm, width, height, blueChan, alphaChan);

			// 所有轮廓的中心（包围盒中心）绘制红色十字叉。
			// 布局将包围盒中心平移到图像正中心，故十字即图像中心。
			const double centerRow = height / 2.0;
			const double centerCol = width / 2.0;
			const double span = (std::max)(width, height) - 2.0 * kMarginPx;
			const double halfLen = (std::max)(8.0, span * 0.08);
			const double crossHalfWidth = (std::max)(1.5, span / 400.0);

			HObject crossH, crossV, cross;
			GenRectangle2(&crossH, centerRow, centerCol, 0.0, halfLen, crossHalfWidth);
			GenRectangle2(&crossV, centerRow, centerCol, 1.57079632679489661923, halfLen, crossHalfWidth);
			Union2(crossH, crossV, &cross);

			// R 通道：仅十字处 255；B 通道十字处压 0，保证十字显示为纯红而非紫
			HImage redChan;
			GenImageConst(&redChan, "byte", width, height);
			PaintRegion(cross, redChan, &redChan, 255, "fill");
			PaintRegion(cross, blueChan, &blueChan, 0, "fill");

			// A 通道：十字同样不透明
			PaintRegion(cross, alphaChan, &alphaChan, 255, "fill");

			// 输出 RGBA PNG。Halcon 的 PNG 编码不支持 4 通道，改用 QImage 写出。
			auto getBits = [](const HImage& img)
			{
				HString type;
				Hlong w, h;
				return reinterpret_cast<const unsigned char*>(
					img.GetImagePointer1(&type, &w, &h));
			};
			const unsigned char* rBits = getBits(redChan);
			const unsigned char* bBits = getBits(blueChan);
			const unsigned char* aBits = getBits(alphaChan);

			QImage img(width, height, QImage::Format_RGBA8888);
			for (int y = 0; y < height; ++y)
			{
				uchar* line = img.scanLine(y);
				const size_t rowOff = static_cast<size_t>(y) * width;
				for (int x = 0; x < width; ++x)
				{
					line[x * 4 + 0] = rBits[rowOff + x];
					line[x * 4 + 1] = 0;
					line[x * 4 + 2] = bBits[rowOff + x];
					line[x * 4 + 3] = aBits[rowOff + x];
				}
			}

			// Halcon read_image 对"二值 alpha（恰好两个灰度值）"的 PNG 会丢弃 alpha
			// 通道并返回缩减 domain 的 3 通道图。此处将角点 alpha 置为 1（视觉上
			// 完全透明），使 alpha 含 3 个取值，保证读回时保留 4 通道 RGBA，
			// 供 compositeOverlay 叠加使用。
			img.scanLine(0)[3] = 1;

			return img.save(QString::fromStdString(pngPath), "PNG");
		}
		catch (const HException& e)
		{
			std::cerr << "[renderDxfContoursToRgbaPng] Halcon异常: " << e.ErrorMessage().Text() << std::endl;
			return false;
		}
		catch (...)
		{
			return false;
		}
	}

	bool renderDxfContoursPreview(const HalconCpp::HObject& contours,
		HalconCpp::HImage& previewOut, HalconCpp::HTuple& homMatContourToImageOut)
	{
		using namespace HalconCpp;
		try
		{
			ClipRegionGuard clipGuard;

			HTuple hm;
			int width, height;
			if (!computeLayout(contours, hm, width, height))
				return false;

			HImage blueChan, alphaChan;
			paintLineChannels(contours, hm, width, height, blueChan, alphaChan);

			// RGB 预览：黑底蓝线
			HImage black;
			GenImageConst(&black, "byte", width, height);
			Compose3(black, black, blueChan, &previewOut);

			homMatContourToImageOut = hm;
			return true;
		}
		catch (const HException& e)
		{
			std::cerr << "[renderDxfContoursPreview] Halcon异常: " << e.ErrorMessage().Text() << std::endl;
			return false;
		}
		catch (...)
		{
			return false;
		}
	}

	bool renderDxfToRgbaPng(const std::string& dxfPath, const std::string& pngPath)
	{
		HalconCpp::HObject contours;
		if (!loadDxfContours(dxfPath, contours))
			return false;
		return renderDxfContoursToRgbaPng(contours, pngPath);
	}
}
