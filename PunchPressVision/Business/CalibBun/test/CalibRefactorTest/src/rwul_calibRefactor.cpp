#include "rwul_calibRefactor.hpp"

#include <filesystem>

#include "opencv2/calib3d.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

namespace rwul
{

    void showCvMatDebugWindow(const cv::Mat & mat,const std::string & windowName,cv::Size size)
    {
        cv::namedWindow(windowName, cv::WINDOW_NORMAL);
        cv::resizeWindow(windowName, size.width, size.height);
        cv::imshow(windowName, mat);
        cv::waitKey(0);
    }

	void calibMask(const cv::Mat& mat)
	{
        const int flags = cv::CALIB_CB_SYMMETRIC_GRID;

        cv::Size boardSize_{7,7};
        
        cv::Mat gray;
        if (mat.channels() == 1)
        {
            gray = mat.clone();
        }
        else
        {
            cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);
        }

        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 0);

        cv::Mat binary;


        std::vector<cv::Point2f> corners;

        auto found = cv::findCirclesGrid(
            mat, boardSize_, corners,
            flags | cv::CALIB_CB_CLUSTERING);

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

            params.blobColor = 0;
            cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);
            found = cv::findCirclesGrid(mat, boardSize_, corners,
                flags | cv::CALIB_CB_CLUSTERING, detector);

            if (!found)
            {
                params.blobColor = 255;
                detector = cv::SimpleBlobDetector::create(params);
                found = cv::findCirclesGrid(mat, boardSize_, corners,
                    flags | cv::CALIB_CB_CLUSTERING, detector);
            }
        }

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

            showCvMatDebugWindow(display,"test",{1920,1080});
        }
	}
}
