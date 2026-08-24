#include "StampPatternModule.t.hpp"

#include <iostream>
#include <filesystem>

#include "infrastructure/StampPatternModule/Config/StampPatternItem.hpp"
#include "infrastructure/StampPatternModule/DxfPatternRenderer.hpp"

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

        // ===== DXF 导入：生成矩形轮廓写成 DXF，验证渲染导入 =====
        const fs::path dxfPath = fs::temp_directory_path() / "stamp_pattern_test_src.dxf";
        try
        {
            HalconCpp::HObject rect;
            HalconCpp::GenRectangle1(&rect, 0, 0, 30, 50);
            HalconCpp::HObject contour;
            HalconCpp::GenContourRegionXld(rect, &contour, "border");
            HalconCpp::WriteContourXldDxf(contour, dxfPath.string().c_str());
        }
        catch (const HalconCpp::HException& e)
        {
            std::cout << "generate dxf failed: " << e.ErrorMessage().Text() << std::endl;
        }
        std::cout << "dxf exists=" << fs::exists(dxfPath) << std::endl;

        // ===== 区域过滤 + 预览 API =====
        {
            HalconCpp::HObject contours;
            if (inf::loadDxfContours(dxfPath.string(), contours))
            {
                HalconCpp::HTuple n;
                HalconCpp::CountObj(contours, &n);
                std::cout << "loadDxfContours count=" << n.I() << std::endl;

                // 诊断：打印轮廓包围盒
                {
                    HalconCpp::HTuple br1, bc1, br2, bc2;
                    HalconCpp::SmallestRectangle1Xld(contours, &br1, &bc1, &br2, &bc2);
                    std::cout << "contour bbox rows=" << br1[0].D() << ".." << br2[0].D()
                              << " cols=" << bc1[0].D() << ".." << bc2[0].D() << std::endl;
                }

                // 矩形（镜像后 rows -30..0, cols 0..50）：区域内应选中 1 个
                std::vector<inf::DxfRect> hit{ {-40, -10, 10, 60} };
                HalconCpp::HObject sel;
                inf::filterDxfContoursByRegions(contours, hit, sel);
                HalconCpp::CountObj(sel, &n);
                std::cout << "filter hit count=" << n.I() << std::endl;

                // 远处区域：应选中 0 个
                std::vector<inf::DxfRect> miss{ {1000, 1000, 1100, 1100} };
                inf::filterDxfContoursByRegions(contours, miss, sel);
                HalconCpp::CountObj(sel, &n);
                std::cout << "filter miss count=" << n.I() << std::endl;

                // 预览：RGB 3 通道 + 6 元仿射
                HalconCpp::HImage preview;
                HalconCpp::HTuple hm;
                if (inf::renderDxfContoursPreview(contours, preview, hm))
                {
                    std::cout << "preview channels=" << preview.CountChannels().I()
                              << " hommatLen=" << hm.TupleLength().I() << std::endl;
                }
            }
        }

        auto dxfInfo = manager.importStampPattern(dxfPath.string(), "TestDxfPattern");
        std::cout << "dxf imported id=" << dxfInfo.getId() << std::endl;

        auto dxfItem = manager.getStampPatternItem(dxfInfo.getId());
        std::cout << "dxf imageInitialized=" << dxfItem.data._patternImage.IsInitialized() << std::endl;
        if (dxfItem.data._patternImage.IsInitialized())
        {
            const int w = dxfItem.data._patternImage.Width().I();
            const int h = dxfItem.data._patternImage.Height().I();
            std::cout << " channels=" << dxfItem.data._patternImage.CountChannels().I()
                      << " width=" << w
                      << " height=" << h << std::endl;

            // 校验渲染样式：中心十字为纯红不透明；线条为蓝色；背景透明
            HalconCpp::HImage R, G, B, A;
            HalconCpp::Decompose4(dxfItem.data._patternImage, &R, &G, &B, &A);
            const int cy = h / 2, cx = w / 2;
            std::cout << " centerRGBA=" << R.GetGrayval(cy, cx).I()
                      << "," << G.GetGrayval(cy, cx).I()
                      << "," << B.GetGrayval(cy, cx).I()
                      << "," << A.GetGrayval(cy, cx).I() << std::endl;
            try
            {
                HalconCpp::HTuple bMin, bMax, bRange, aMin, aMax, aRange;
                HalconCpp::HObject fullRegion;
                HalconCpp::GenRectangle1(&fullRegion, 0, 0, h - 1, w - 1);
                HalconCpp::MinMaxGray(fullRegion, B, 0, &bMin, &bMax, &bRange);
                HalconCpp::MinMaxGray(fullRegion, A, 0, &aMin, &aMax, &aRange);
                std::cout << " blueLineMax=" << bMax.D()
                          << " alphaBgMin=" << aMin.D() << std::endl;
            }
            catch (const HalconCpp::HException& e)
            {
                std::cout << "minmax failed: " << e.ErrorMessage().Text() << std::endl;
            }
        }

        // 保留一份渲染结果到临时目录，便于人工查看
        {
            std::error_code ec2;
            fs::copy_file(dxfInfo.getFolderPath() + "/pattern.png",
                (fs::temp_directory_path() / "stamp_pattern_test_dxf_render.png").string(),
                fs::copy_options::overwrite_existing, ec2);
        }

        manager.deleteStampPattern(dxfInfo.getId());

        manager.destroy();

        // 清理临时文件
        std::error_code ec;
        fs::remove(srcPath, ec);
        fs::remove(dxfPath, ec);
    }
}

int main(int argc, char* argv[])
{
    // 传入 DXF 路径时：直接渲染到临时目录并保留结果，便于用真实图纸人工验证
    if (argc > 1)
    {
        const std::string dxfPath = argv[1];
        const std::string outPath =
            (std::filesystem::temp_directory_path() / "stamp_pattern_dxf_manual_render.png").string();

        // 诊断：轮廓数量与整体包围盒
        {
            HalconCpp::HObject contours;
            if (inf::loadDxfContours(dxfPath, contours))
            {
                HalconCpp::HTuple n;
                HalconCpp::CountObj(contours, &n);
                HalconCpp::HTuple r1, c1, r2, c2;
                HalconCpp::SmallestRectangle1Xld(contours, &r1, &c1, &r2, &c2);
                std::cout << "diag count=" << n.I()
                          << " rows=" << r1.TupleMin().D() << ".." << r2.TupleMax().D()
                          << " cols=" << c1.TupleMin().D() << ".." << c2.TupleMax().D()
                          << std::endl;

                // 模拟对话框框选流程：预览仿射 -> 逆变换映射图像矩形 ->
                // 过滤 -> 渲染，验证裁切管线
                HalconCpp::HImage preview;
                HalconCpp::HTuple hm;
                if (inf::renderDxfContoursPreview(contours, preview, hm))
                {
                    HalconCpp::HTuple inv;
                    HalconCpp::HomMat2dInvert(hm, &inv);

                    // 图像坐标系下框选左下角一只鞋（按 1991x701 布局估算）
                    HalconCpp::HTuple ro, co;
                    HalconCpp::AffineTransPoint2d(inv,
                        HalconCpp::HTuple(400.0).TupleConcat(690.0),
                        HalconCpp::HTuple(100.0).TupleConcat(400.0), &ro, &co);

                    inf::DxfRect rc;
                    rc.row1 = (std::min)(ro[0].D(), ro[1].D());
                    rc.row2 = (std::max)(ro[0].D(), ro[1].D());
                    rc.col1 = (std::min)(co[0].D(), co[1].D());
                    rc.col2 = (std::max)(co[0].D(), co[1].D());
                    std::cout << "crop contourRect rows=" << rc.row1 << ".." << rc.row2
                              << " cols=" << rc.col1 << ".." << rc.col2 << std::endl;

                    HalconCpp::HObject sel;
                    inf::filterDxfContoursByRegions(contours, { rc }, sel);
                    HalconCpp::CountObj(sel, &n);
                    std::cout << "crop selected=" << n.I() << std::endl;

                    const std::string cropPath =
                        (std::filesystem::temp_directory_path() / "stamp_pattern_dxf_crop_render.png").string();
                    const bool ok = inf::renderDxfContoursToRgbaPng(sel, cropPath);
                    std::cout << "crop render -> " << (ok ? cropPath : "FAILED") << std::endl;
                }
            }
        }

        const bool ok = inf::renderDxfToRgbaPng(dxfPath, outPath);
        std::cout << "render " << dxfPath << " -> " << (ok ? outPath : "FAILED") << std::endl;
        return ok ? 0 : 1;
    }

    test::StampPatternModuleTest::runBasicTest();
    return 0;
}
