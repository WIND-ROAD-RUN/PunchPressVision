#pragma once

#include <string>
#include <vector>

#include "halconcpp/HalconCpp.h"

namespace inf
{
	// DXF 轮廓坐标系中的矩形区域（row1, col1, row2, col2）
	struct DxfRect
	{
		double row1 = 0;
		double col1 = 0;
		double row2 = 0;
		double col2 = 0;
	};

	// 读取 DXF 为 XLD 轮廓并恢复 CAD 看图朝向（垂直镜像）。
	// 解析失败或无有效轮廓返回 false。
	bool loadDxfContours(const std::string& dxfPath, HalconCpp::HObject& contoursOut);

	// 按区域过滤轮廓：保留包围盒中心落入任一矩形内的轮廓。
	// rects 为空时返回全部轮廓。结果可能为空对象（调用方用 count_obj 判断）。
	void filterDxfContoursByRegions(const HalconCpp::HObject& contours,
		const std::vector<DxfRect>& rects, HalconCpp::HObject& selectedOut);

	// 将（已过滤的）轮廓渲染为 RGBA 套版图片，输出为 PNG。
	// 按包围盒自适应尺寸并限制最大边长；黑色透明背景、蓝色线条，
	// 所有轮廓的中心（包围盒中心）绘制红色十字叉。
	bool renderDxfContoursToRgbaPng(const HalconCpp::HObject& contours, const std::string& pngPath);

	// 渲染预览图（RGB：黑底蓝线，无十字），并输出 轮廓坐标 -> 图像坐标 的仿射变换，
	// 供交互区域选择将屏幕框选映射回轮廓坐标系。
	bool renderDxfContoursPreview(const HalconCpp::HObject& contours,
		HalconCpp::HImage& previewOut, HalconCpp::HTuple& homMatContourToImageOut);

	// 一步到位：读取 DXF 并渲染全部轮廓为 RGBA PNG（等价 load + render）。
	bool renderDxfToRgbaPng(const std::string& dxfPath, const std::string& pngPath);
}
