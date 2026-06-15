#include "OpenCvCalibrator.hpp"

#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/features2d.hpp>

#include <filesystem>
#include <sstream>

OpenCvCalibrator::OpenCvCalibrator()
    : boardSize_(7, 7)
    , squareSize_(20.0)
    , pattern_(CalibrationPattern::SymmetricCircles)
{
    cameraMatrix_ = cv::Mat::eye(3, 3, CV_64F);
}

void OpenCvCalibrator::setBoardSize(const cv::Size& boardSize, double squareSize, CalibrationPattern pattern)
{
    boardSize_ = boardSize;
    squareSize_ = squareSize;
    pattern_ = pattern;
    clearCalibrationImages();
}

std::vector<cv::Point3f> OpenCvCalibrator::generateObjectPoints() const
{
    std::vector<cv::Point3f> points;
    points.reserve(static_cast<size_t>(boardSize_.width) * boardSize_.height);

    if (pattern_ == CalibrationPattern::AsymmetricCircles)
    {
        for (int i = 0; i < boardSize_.height; ++i)
        {
            for (int j = 0; j < boardSize_.width; ++j)
            {
                points.emplace_back(
                    static_cast<float>(j * squareSize_),
                    static_cast<float>((2 * i + j % 2) * squareSize_),
                    0.0f);
            }
        }
    }
    else
    {
        // 棋盘格与对称圆点：规则网格
        for (int i = 0; i < boardSize_.height; ++i)
        {
            for (int j = 0; j < boardSize_.width; ++j)
            {
                points.emplace_back(
                    static_cast<float>(j * squareSize_),
                    static_cast<float>(i * squareSize_),
                    0.0f);
            }
        }
    }

    return points;
}

bool OpenCvCalibrator::addCalibrationImage(const cv::Mat& image)
{
    if (image.empty())
        return false;

    imageSize_ = image.size();

    std::vector<cv::Point2f> corners;
    bool found = false;

    if (pattern_ == CalibrationPattern::Chessboard)
    {
        cv::Mat gray;
        if (image.channels() == 1)
            gray = image;
        else
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

        found = cv::findChessboardCorners(
            gray, boardSize_, corners,
            cv::CALIB_CB_ADAPTIVE_THRESH |
            cv::CALIB_CB_NORMALIZE_IMAGE |
            cv::CALIB_CB_FAST_CHECK);

        if (found)
        {
            cv::cornerSubPix(
                gray, corners,
                cv::Size(11, 11),
                cv::Size(-1, -1),
                cv::TermCriteria(
                    cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER,
                    30, 0.001));
        }
    }
    else
    {
        // 对称/非对称圆点：findCirclesGrid 内部会自动处理灰度转换
        const int flags = (pattern_ == CalibrationPattern::SymmetricCircles)
            ? cv::CALIB_CB_SYMMETRIC_GRID
            : cv::CALIB_CB_ASYMMETRIC_GRID;

        // 1. 先尝试 OpenCV 默认 blob 检测器
        found = cv::findCirclesGrid(
            image, boardSize_, corners,
            flags | cv::CALIB_CB_CLUSTERING);

        // 2. 失败后使用 SimpleBlobDetector 重试（分别尝试黑圆、白圆）
        if (!found)
        {
            cv::SimpleBlobDetector::Params params;
            params.filterByArea = true;
            params.minArea = 50.0f;
            params.maxArea = static_cast<float>(imageSize_.area()) / 10.0f;
            params.filterByCircularity = true;
            params.minCircularity = 0.7f;
            params.filterByConvexity = true;
            params.minConvexity = 0.8f;
            params.filterByInertia = true;
            params.minInertiaRatio = 0.4f;

            // 尝试黑圆（白底黑圆标定板）
            params.blobColor = 0;
            cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
            found = cv::findCirclesGrid(image, boardSize_, corners,
                flags | cv::CALIB_CB_CLUSTERING, detector);

            // 尝试白圆（黑底白圆标定板）
            if (!found)
            {
                params.blobColor = 255;
                detector = cv::SimpleBlobDetector::create(params);
                found = cv::findCirclesGrid(image, boardSize_, corners,
                    flags | cv::CALIB_CB_CLUSTERING, detector);
            }
        }
    }

    if (!found)
        return false;

    objectPoints_.push_back(generateObjectPoints());
    imagePoints_.push_back(std::move(corners));

    // 保存带特征点标记的预览图
    cv::Mat display;
    if (image.channels() == 1)
        cv::cvtColor(image, display, cv::COLOR_GRAY2BGR);
    else
        display = image.clone();

    cv::drawChessboardCorners(display, boardSize_, imagePoints_.back(), true);
    cornerImages_.push_back(std::move(display));

    return true;
}

void OpenCvCalibrator::clearCalibrationImages()
{
    objectPoints_.clear();
    imagePoints_.clear();
    cornerImages_.clear();
    isCalibrated_ = false;
    rms_ = -1.0;
    cameraMatrix_ = cv::Mat::eye(3, 3, CV_64F);
    distCoeffs_.release();
    map1_.release();
    map2_.release();
}

size_t OpenCvCalibrator::calibrationImageCount() const
{
    return imagePoints_.size();
}

size_t OpenCvCalibrator::cornerImageCount() const
{
    return cornerImages_.size();
}

cv::Mat OpenCvCalibrator::getCornerImage(size_t index) const
{
    if (index >= cornerImages_.size())
        return cv::Mat();

    return cornerImages_[index].clone();
}

double OpenCvCalibrator::calibrate()
{
    if (imagePoints_.size() < 3)
        return -1.0;

    cameraMatrix_ = cv::Mat::eye(3, 3, CV_64F);
    distCoeffs_.release();

    std::vector<cv::Mat> rvecs, tvecs;
    rms_ = cv::calibrateCamera(
        objectPoints_,
        imagePoints_,
        imageSize_,
        cameraMatrix_,
        distCoeffs_,
        rvecs,
        tvecs,
        cv::CALIB_RATIONAL_MODEL,
        cv::TermCriteria(
            cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER,
            100, 1e-6));

    if (rms_ >= 0.0)
    {
        isCalibrated_ = true;
        updateUndistortMaps();
    }

    return rms_;
}

bool OpenCvCalibrator::isCalibrated() const
{
    return isCalibrated_;
}

void OpenCvCalibrator::updateUndistortMaps()
{
    if (!isCalibrated_ || imageSize_.area() <= 0)
        return;

    cv::initUndistortRectifyMap(
        cameraMatrix_,
        distCoeffs_,
        cv::Mat(),
        cameraMatrix_,
        imageSize_,
        CV_16SC2,
        map1_,
        map2_);
}

cv::Mat OpenCvCalibrator::undistort(const cv::Mat& src) const
{
    if (!isCalibrated_ || map1_.empty() || map2_.empty())
        return src.clone();

    cv::Mat dst;
    cv::remap(src, dst, map1_, map2_, cv::INTER_LINEAR);
    return dst;
}

bool OpenCvCalibrator::saveParameters(const std::string& path) const
{
    try
    {
        cv::FileStorage fs(path, cv::FileStorage::WRITE);
        if (!fs.isOpened())
            return false;

        fs << "image_width" << imageSize_.width;
        fs << "image_height" << imageSize_.height;
        fs << "board_width" << boardSize_.width;
        fs << "board_height" << boardSize_.height;
        fs << "square_size" << squareSize_;
        fs << "camera_matrix" << cameraMatrix_;
        fs << "distortion_coefficients" << distCoeffs_;
        fs << "rms" << rms_;
        fs.release();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool OpenCvCalibrator::loadParameters(const std::string& path)
{
    try
    {
        cv::FileStorage fs(path, cv::FileStorage::READ);
        if (!fs.isOpened())
            return false;

        fs["image_width"] >> imageSize_.width;
        fs["image_height"] >> imageSize_.height;
        fs["board_width"] >> boardSize_.width;
        fs["board_height"] >> boardSize_.height;
        fs["square_size"] >> squareSize_;
        fs["camera_matrix"] >> cameraMatrix_;
        fs["distortion_coefficients"] >> distCoeffs_;
        fs["rms"] >> rms_;
        fs.release();

        isCalibrated_ = !cameraMatrix_.empty() && !distCoeffs_.empty() && imageSize_.area() > 0;
        if (isCalibrated_)
            updateUndistortMaps();

        return isCalibrated_;
    }
    catch (...)
    {
        return false;
    }
}

cv::Mat OpenCvCalibrator::cameraMatrix() const
{
    return cameraMatrix_.clone();
}

cv::Mat OpenCvCalibrator::distCoeffs() const
{
    return distCoeffs_.clone();
}

double OpenCvCalibrator::rms() const
{
    return rms_;
}
