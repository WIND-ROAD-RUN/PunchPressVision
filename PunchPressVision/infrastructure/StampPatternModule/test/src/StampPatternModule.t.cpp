#include "StampPatternModule.t.hpp"

#include <iostream>
#include <filesystem>

#include "infrastructure/StampPatternModule/Config/StampPatternItem.hpp"

namespace test
{
    void StampPatternModuleTest::runBasicTest()
    {
        namespace fs = std::filesystem;

        // 生成一张临时源图（供 import 原始复制）
        const fs::path srcPath = fs::temp_directory_path() / "stamp_pattern_test_src.png";
        try
        {
            HalconCpp::HImage img;
            HalconCpp::GenImageConst(&img, "byte", 64, 64);
            img.WriteImage("png", 0, srcPath.string().c_str());
        }
        catch (...) {}

        inf::StampPatternModule manager;
        manager.build();

        // 增
        auto info = manager.importStampPattern(srcPath.string(), "TestPattern");
        std::cout << "imported id=" << info.getId()
                  << " count=" << manager.getStampPatternInfos().size() << std::endl;

        // 查（单个，含图片）
        auto item = manager.getStampPatternItem(info.getId());
        std::cout << "loaded name=" << item.info.base_info.name
                  << " imageInitialized=" << item.data._patternImage.IsInitialized() << std::endl;

        // 改（对齐参数 / 透明度）
        Config::StampPatternData data;
        data.alignRow = 10.0;
        data.alignCol = 20.0;
        data.alignAngle = 0.5;
        data.alignScale = 1.2;
        data.alpha = 200;
        manager.changeStampPattern(info.getId(), data);

        // 改（重命名）
        manager.changeStampPattern(info.getId(), "TestPatternRenamed");

        // 删
        manager.deleteStampPattern(info.getId());
        std::cout << "after delete count=" << manager.getStampPatternInfos().size() << std::endl;

        manager.destroy();

        // 清理临时源图
        std::error_code ec;
        fs::remove(srcPath, ec);
    }
}

int main()
{
    test::StampPatternModuleTest::runBasicTest();
    return 0;
}
