#include "rwul_calibRefactor.hpp"

#include <filesystem>

#include "opencv2/calib3d.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

namespace rwul
{

	void calibMask(const cv::Mat& mat)
	{
        // 对称/非对称圆点：findCirclesGrid 内部会自动处理灰度转换
        const int flags = cv::CALIB_CB_SYMMETRIC_GRID;

        cv::Size boardSize_{7,7};


        std::vector<cv::Point2f> corners;

        // 1. 先尝试 OpenCV 默认 blob 检测器
        auto found = cv::findCirclesGrid(
            mat, boardSize_, corners,
            flags | cv::CALIB_CB_CLUSTERING);

        // 2. 失败后使用 SimpleBlobDetector 重试（分别尝试黑圆、白圆）
        if (!found)
        {
            cv::SimpleBlobDetector::Params params;
            params.filterByArea = true;
            params.minArea = 50.0f;
            params.maxArea = static_cast<float>(mat.size().area()) / 10.0f;
            params.filterByCircularity = true;
            params.minCircularity = 0.7f;
            params.filterByConvexity = true;
            params.minConvexity = 0.8f;
            params.filterByInertia = true;
            params.minInertiaRatio = 0.4f;

            // 尝试黑圆（白底黑圆标定板）
            params.blobColor = 0;
            cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
            found = cv::findCirclesGrid(mat, boardSize_, corners,
                flags | cv::CALIB_CB_CLUSTERING, detector);

            // 尝试白圆（黑底白圆标定板）
            if (!found)
            {
                params.blobColor = 255;
                detector = cv::SimpleBlobDetector::create(params);
                found = cv::findCirclesGrid(mat, boardSize_, corners,
                    flags | cv::CALIB_CB_CLUSTERING, detector);
            }
        }

        // 调试：如果全部失败，保存 SimpleBlobDetector 找到的 blobs
        if (!found)
        {
            cv::SimpleBlobDetector::Params debugParams;
            debugParams.filterByArea = true;
            debugParams.minArea = 10.0f;
            debugParams.maxArea = static_cast<float>(mat.size().area()) / 5.0f;
            debugParams.filterByCircularity = true;
            debugParams.minCircularity = 0.5f;
            debugParams.filterByConvexity = false;
            debugParams.filterByInertia = false;

            cv::Ptr<cv::SimpleBlobDetector> debugDetector = cv::SimpleBlobDetector::create(debugParams);
            std::vector<cv::KeyPoint> keypoints;
            debugDetector->detect(mat, keypoints);

            cv::Mat display;
            if (mat.channels() == 1)
                cv::cvtColor(mat, display, cv::COLOR_GRAY2BGR);
            else
                display = mat.clone();

            cv::drawKeypoints(display, keypoints, display, cv::Scalar(0, 0, 255),
                cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

            cv::imshow("test", display);
            cv::waitKey(0);
        }
	}
}
