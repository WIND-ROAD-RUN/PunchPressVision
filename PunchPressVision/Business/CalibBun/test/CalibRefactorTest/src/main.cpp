#include "rwul_calibRefactor.hpp"

#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    const char* path = (argc > 1) ? argv[1] : "calib.png";

    cv::Mat img = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img.empty())
    {
        std::cerr << "Failed to load image: " << path << std::endl;
        return -1;
    }

    rwul::calibMask(img);

    std::cout << "calibMask called successfully for " << path << std::endl;
    return 0;
}
