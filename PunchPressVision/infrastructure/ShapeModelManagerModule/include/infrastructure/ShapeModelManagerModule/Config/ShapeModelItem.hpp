#pragma once
#include <string>
#include <vector>

#include "halconcpp/HalconCpp.h"

namespace Config
{
	struct ShapeModelData
	{
		// 创建的模板的图片
		HalconCpp::HImage _templateMatImage;

		// 创建的原始图片
		HalconCpp::HImage _originalImage;

		// 带 ROI/Mask 标注的预览图（用于模型库预览）
		HalconCpp::HImage _annotatedImage;

		HalconCpp::HTuple hv_ModelID;
		HalconCpp::HTuple hv_MetrologyHandle;

		// 用数组记录每一次绘制的 ROI/轮廓，方便回撤
		std::vector<HalconCpp::HObject> _paintCreateRoiList;
		std::vector<HalconCpp::HObject> _paintShieldRoiList;

		// 存储找到的轮廓（XLD），用于显示/调试（DispObj）
		HalconCpp::HObject _findCreateXldObj;
		// 存储其他匹配轮廓（XLD），用于显示不同颜色（青色）
		HalconCpp::HObject _findCreateXldObj_Secondary;

		// 自定义的中心点坐标
		double centerX = 0;
		double centerY = 0;
		// 模板匹配找到的中心点坐标
		double findCenterX = 0;
		double findCenterY = 0;

		// 左右偏差（X 方向，单位：mm 或像素，取决于业务定义）
		double offsetX = 0;
		// 前后偏差（Y 方向，单位：mm 或像素，取决于业务定义）
		double offsetY = 0;
		// 角度偏差（单位：deg 或 rad，取决于业务定义）
		double offsetAngle = 0;

		int findnumber = 0;

		// 创建模板时的曝光、增益、光源
		double _createModelExposureTime = 0.0;
		double _createModelGain = 0.0;
		bool upperLight = false;
		bool lowerLight = false;

		// 图像预处理参数
		int _createModelPreProcessType = 0;      // 对应 comboBox_ImageType->currentIndex()

		bool _createModelUseOpening = false;
		int _createModelOpeningRadius = 0;

		bool _createModelUseClosing = false;
		int _createModelClosingRadius = 0;

		bool _createModelUseMean = false;
		int _createModelMeanRadius = 0;

		// 训练参数
		int numLevels = 4;
		double angleStart = -3.14;
		double angleExtent = 6.28;
		double angleStep = 0.0;                  // 0 = auto
		std::string optimization = "auto";
		std::string metric = "use_polarity";
		int contrast = 30;
		int minContrast = 10;

		std::string modelPath{};

	public:
		void loadInDir(const std::string& dir);
		void saveInDir(const std::string& dir);
	};

	struct ShapeModelInfo
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

	struct ShapeModelItem
	{
	public:
		ShapeModelInfo info;
	public:
		ShapeModelData data;
	public:
		void loadInDir(const std::string& dir);
		void saveInDir(const std::string& dir);
	};
}
