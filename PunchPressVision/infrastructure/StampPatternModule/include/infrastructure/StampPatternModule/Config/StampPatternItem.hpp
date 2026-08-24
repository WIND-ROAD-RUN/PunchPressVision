#pragma once
#include <string>

#include "halconcpp/HalconCpp.h"

namespace Config
{
	// 套版图片在套版目录中的固定文件名
	inline constexpr const char* kPatternImageFileName = "pattern.png";

	// 原始复制图片文件（字节级，保留透明通道），返回是否成功
	bool copyImageFile(const std::string& src, const std::string& dst);

	struct StampPatternData
	{
		// 用户导入的套版图案图（带透明通道，RGBA 4 通道）
		HalconCpp::HImage _patternImage;

		// 手动对齐变换 A：套版图坐标 -> 参考(训练)图像坐标。
		// alignRow/alignCol 为平移（参考图像坐标），alignAngle 为旋转角（弧度），alignScale 为缩放。
		double alignRow = 0;
		double alignCol = 0;
		double alignAngle = 0;
		double alignScale = 1.0;

		// 叠加透明度 0~255（255 = 完全不透明）
		int alpha = 180;

		// 是否来源于 CAD DXF 图纸。true 时套版图像素单位 = DXF 实际尺寸单位（mm），
		// 叠加显示需按九点标定换算为像素尺寸（effectiveScale = alignScale × pixelsPerMm）；
		// false 时套版图为普通图片，1 像素 = 1 像素。
		bool fromDxf = false;

	public:
		void loadInDir(const std::string& dir);
		void saveInDir(const std::string& dir);
	};

	struct StampPatternInfo
	{
	public:
		struct BaseInfo
		{
			std::string name;
		};
	public:
		BaseInfo base_info;
	private:
		std::string id_;
		std::string create_time_;
		std::string update_time_;
		std::string folder_path_;
	public:
		std::string getId() const { return id_; }
		std::string getCreateTime() const { return create_time_; }
		std::string getUpdateTime() const { return update_time_; }
		std::string getFolderPath() const { return folder_path_; }
		void setId(const std::string& id) { this->id_ = id; }
		void setCreateTime(const std::string& createTime) { this->create_time_ = createTime; }
		void setUpdateTime(const std::string& updateTime) { this->update_time_ = updateTime; }
		void setFolderPath(const std::string& folderPath) { this->folder_path_ = folderPath; }
	public:
		void loadInDir(const std::string& dir);
		void saveInDir(const std::string& dir);
	};

	struct StampPatternItem
	{
	public:
		StampPatternInfo info;
	public:
		StampPatternData data;
	public:
		void loadInDir(const std::string& dir);
		void saveInDir(const std::string& dir);
	};
}
