#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

/**
 * @brief 标定板图案类型。
 */
enum class CalibrationPattern
{
    Chessboard,
    SymmetricCircles,
    AsymmetricCircles
};

/**
 * @brief 基于 OpenCV 圆点/棋盘格的独立标定器。
 *
 * 该类不依赖 CalibBun、CalibConfigModule 或任何 Halcon 标定流程，
 * 仅使用 OpenCV 的 findCirclesGrid / findChessboardCorners / calibrateCamera / remap 完成：
 *   - 从图像中提取标定板特征点
 *   - 相机内参、畸变系数求解
 *   - 标定参数 YAML 序列化
 *   - 图像畸变矫正
 */
class OpenCvCalibrator
{
public:
    OpenCvCalibrator();

    /**
     * @brief 设置标定板参数。
     * @param boardSize  内角点/圆心数量（列 x 行）。
     * @param squareSize 相邻圆心间距，单位 mm。
     * @param pattern    标定板图案类型，默认对称圆点。
     */
    void setBoardSize(const cv::Size& boardSize, double squareSize, CalibrationPattern pattern = CalibrationPattern::SymmetricCircles);

    /**
     * @brief 添加一张标定图像，自动检测特征点。
     * @param image 输入图像，任意通道。
     * @return 是否成功检测到特征点。
     */
    bool addCalibrationImage(const cv::Mat& image);

    /** @brief 清空已缓存的标定图像和特征点。 */
    void clearCalibrationImages();

    /** @brief 当前已缓存的有效标定图像数量。 */
    size_t calibrationImageCount() const;

    /**
     * @brief 获取绘制了特征点的预览图数量。
     * @note 仅包含成功检测到特征点的图像。
     */
    size_t cornerImageCount() const;

    /**
     * @brief 获取指定索引的特征点预览图。
     * @param index 成功检测到特征点的图像索引（0 ~ cornerImageCount()-1）。
     * @return 绘制了特征点的 BGR 图像。索引无效时返回空 Mat。
     */
    cv::Mat getCornerImage(size_t index) const;

    /**
     * @brief 执行相机标定。
     * @return RMS 重投影误差（像素）。失败返回负值。
     */
    double calibrate();

    /** @brief 是否已完成标定。 */
    bool isCalibrated() const;

    /**
     * @brief 对单张图像进行畸变矫正。
     * @param src 原始图像。
     * @return 矫正后的图像。未标定时返回原图拷贝。
     */
    cv::Mat undistort(const cv::Mat& src) const;

    /**
     * @brief 保存标定参数到 YAML 文件。
     * @param path 文件路径。
     * @return 是否成功。
     */
    bool saveParameters(const std::string& path) const;

    /**
     * @brief 从 YAML 文件加载标定参数。
     * @param path 文件路径。
     * @return 是否成功。
     */
    bool loadParameters(const std::string& path);

    cv::Mat cameraMatrix() const;
    cv::Mat distCoeffs() const;
    double rms() const;

private:
    std::vector<cv::Point3f> generateObjectPoints() const;
    void updateUndistortMaps();

private:
    cv::Size boardSize_;
    double squareSize_ = 20.0;
    CalibrationPattern pattern_ = CalibrationPattern::SymmetricCircles;
    cv::Size imageSize_;

    std::vector<std::vector<cv::Point3f>> objectPoints_;
    std::vector<std::vector<cv::Point2f>> imagePoints_;
    std::vector<cv::Mat> cornerImages_;

    cv::Mat cameraMatrix_;
    cv::Mat distCoeffs_;
    cv::Mat map1_;
    cv::Mat map2_;

    double rms_ = -1.0;
    bool isCalibrated_ = false;
};
