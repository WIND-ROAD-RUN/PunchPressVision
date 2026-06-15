#include "rwul_calibRefactor.hpp"

#include "opencv2/features2d.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

namespace rwul
{
	void calibMask(const cv::Mat& mat)
	{
        auto imgSize = mat.size();

        cv::SimpleBlobDetector::Params debugParams;
        debugParams.filterByArea = true;
        debugParams.minArea = 1000.0f;
        debugParams.maxArea = static_cast<float>(imgSize.area()) / 5.0f;
        debugParams.filterByCircularity = true;
        debugParams.minCircularity = 0.5f;
        debugParams.filterByConvexity = false;
        debugParams.filterByInertia = false;
        debugParams.blobColor = 255;

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

        cv::namedWindow("test", cv::WINDOW_NORMAL);
        cv::resizeWindow("test", 1920, 1080);
        cv::imshow("test", display);
        cv::waitKey(0);
	}
}
