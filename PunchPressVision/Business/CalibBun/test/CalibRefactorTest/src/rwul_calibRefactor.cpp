#include "rwul_calibRefactor.hpp"

#include "opencv2/features2d.hpp"

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

        //saveDebugImage(binary, keypoints, "binary_blobs");
	}
}
