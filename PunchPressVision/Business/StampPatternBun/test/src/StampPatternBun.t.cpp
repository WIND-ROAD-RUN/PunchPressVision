#include "StampPatternBun.t.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

#include <QCoreApplication>

#include "Business/StampPatternBun/StampPatternBun.hpp"

namespace
{
	// 生成一张不透明红色 RGBA 图像，用于直接验证叠加合成路径
	HalconCpp::HImage makeOpaqueRedRgba(int w, int h)
	{
		HalconCpp::HImage ch[4];
		for (int i = 0; i < 4; ++i)
			HalconCpp::GenImageConst(&ch[i], "byte", w, h);

		// R = 255
		HalconCpp::HObject dom;
		HalconCpp::GetDomain(ch[0], &dom);
		HalconCpp::OverpaintRegion(ch[0], dom, 255, "fill");
		// A = 255（不透明）
		HalconCpp::GetDomain(ch[3], &dom);
		HalconCpp::OverpaintRegion(ch[3], dom, 255, "fill");

		HalconCpp::HImage rgba;
		HalconCpp::Compose4(ch[0], ch[1], ch[2], ch[3], &rgba);
		return rgba;
	}
}

namespace test
{
	void StampPatternBunTest::runBasicTest()
	{
		namespace fs = std::filesystem;

		inf::infrastructure inf;
		inf.build();

		bun::StampPatternBun bun(inf);

		// 生成临时源图（供 import 原始复制）
		const fs::path srcPath = fs::temp_directory_path() / "stamp_pattern_bun_test_src.png";
		try
		{
			HalconCpp::HImage img;
			HalconCpp::GenImageConst(&img, "byte", 64, 64);
			img.WriteImage("png", 0, srcPath.string().c_str());
		}
		catch (...) {}

		// 增
		auto info = bun.importPattern(srcPath.string(), "BunTestPattern");
		std::cout << "bun import id=" << info.getId()
			<< " count=" << bun.getAllPatterns().size() << std::endl;

		// 加载
		bool ok = bun.loadPattern(info.getId());
		std::cout << "bun loadPattern=" << ok
			<< " loadedCount=" << bun.getLoadedPatternCount()
			<< " isLoaded=" << bun.isPatternLoaded(info.getId()) << std::endl;

		// 更新对齐参数 + 读取内存副本
		Config::StampPatternData data;
		bool gotData = bun.getLoadedPatternData(info.getId(), data);
		data.alignRow = 10.0;
		data.alignCol = 20.0;
		data.alignAngle = 0.3;
		data.alignScale = 1.0;
		data.alpha = 200;
		bun.updatePatternData(info.getId(), data);

		Config::StampPatternData readBack;
		bun.getLoadedPatternData(info.getId(), readBack);
		std::cout << "gotData=" << gotData
			<< " readBack alignRow=" << readBack.alignRow
			<< " alignCol=" << readBack.alignCol
			<< " alpha=" << readBack.alpha << std::endl;

		// 重命名
		bun.renamePattern(info.getId(), "BunTestPatternRenamed");

		// 对齐变换 A（9 元组）
		HalconCpp::HTuple H = bun.buildAlignHomMat2D(data);
		std::cout << "align homMat2D len=" << H.TupleLength().I() << std::endl;

		// 叠加合成：不透明红色套版图放到 (40,40)，合成结果应为 3 通道
		HalconCpp::HImage base;
		HalconCpp::GenImageConst(&base, "byte", 128, 128);

		HalconCpp::HTuple HPat2Base;
		HalconCpp::HomMat2dIdentity(&HPat2Base);
		HalconCpp::HomMat2dTranslate(HPat2Base, 40.0, 40.0, &HPat2Base);

		HalconCpp::HImage composite = bun.compositeOverlay(base, makeOpaqueRedRgba(32, 32), HPat2Base, 255);
		bool compositeOk = composite.IsInitialized() && composite.CountChannels().I() == 3;
		std::cout << "composite channels=" << composite.CountChannels().I()
			<< " size=" << composite.Width().I() << "x" << composite.Height().I()
			<< " ok=" << compositeOk << std::endl;

		// 像素级位置断言（ROI 局部合成路径）：
		// 套版 32x32 平移到 (40,40)，中心 (56,56) 应为红色 (255,0,0)，
		// 套版外 (10,10) 应保持底图黑 (0,0,0)
		bool positionOk = false;
		try
		{
			HalconCpp::HTuple gIn = composite.GetGrayval(56, 56);
			HalconCpp::HTuple gOut = composite.GetGrayval(10, 10);
			positionOk = (gIn.TupleLength() == 3 && gOut.TupleLength() == 3
				&& gIn[0].I() == 255 && gIn[1].I() == 0 && gIn[2].I() == 0
				&& gOut[0].I() == 0 && gOut[1].I() == 0 && gOut[2].I() == 0);
			std::cout << "pixel in(56,56)=(" << gIn[0].I() << "," << gIn[1].I() << "," << gIn[2].I() << ")"
				<< " out(10,10)=(" << gOut[0].I() << "," << gOut[1].I() << "," << gOut[2].I() << ")"
				<< " positionOk=" << positionOk << std::endl;
		}
		catch (const HalconCpp::HException& e)
		{
			std::cerr << "GetGrayval 异常: " << e.ErrorMessage().Text() << std::endl;
		}
		compositeOk = compositeOk && positionOk;

		// 卸载 + 删
		bun.unloadPattern(info.getId());
		std::cout << "after unload loadedCount=" << bun.getLoadedPatternCount() << std::endl;
		bun.deletePattern(info.getId());
		std::cout << "after delete count=" << bun.getAllPatterns().size() << std::endl;

		inf.destroy();

		std::error_code ec;
		fs::remove(srcPath, ec);

		// 合成路径失败则返回非零，便于脚本/人工快速判断
		if (!compositeOk)
			std::cerr << "[FAIL] compositeOverlay RGBA 路径未生效（可能被 catch 吞掉）" << std::endl;
	}
}

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	// diag：跳过基础设施构建（避免相机初始化），快速诊断 ROI 合成坐标系
	if (argc > 1 && std::string(argv[1]) == "diag")
	{
		inf::infrastructure inf;
		bun::StampPatternBun bun(inf);

		HalconCpp::HImage base;
		HalconCpp::GenImageConst(&base, "byte", 128, 128);
		HalconCpp::HTuple H;
		HalconCpp::HomMat2dIdentity(&H);
		HalconCpp::HomMat2dTranslate(H, 40.0, 40.0, &H);

		HalconCpp::HImage composite = bun.compositeOverlay(base, makeOpaqueRedRgba(32, 32), H, 255);

		// 期望：红色在 rows/cols 40..71
		const int probePts[][2] = {
			{ 56, 56 },   // 正确位置（套版中心）
			{ 40, 40 },   // 正确位置（套版左上角）
			{ 18, 18 },   // 若混合按局部坐标但贴回原点 (0,0)，这里会红
			{ 96, 96 },   // 参照点
		};
		for (const auto& p : probePts)
		{
			HalconCpp::HTuple g = composite.GetGrayval(p[0], p[1]);
			std::cout << "gray(" << p[0] << "," << p[1] << ")=("
				<< g[0].I() << "," << g[1].I() << "," << g[2].I() << ")" << std::endl;
		}

		// 半透明：alpha=128 时红色通道应约为 128
		{
			HalconCpp::HImage half = bun.compositeOverlay(base, makeOpaqueRedRgba(32, 32), H, 128);
			HalconCpp::HTuple g = half.GetGrayval(56, 56);
			std::cout << "half alpha(56,56)=(" << g[0].I() << "," << g[1].I() << "," << g[2].I()
				<< ") expect ~(128,0,0)" << std::endl;
		}

		// 部分出界：平移到 (-10,-10)，ROI 裁剪路径不应崩溃，左上角应红
		{
			HalconCpp::HTuple Ho;
			HalconCpp::HomMat2dIdentity(&Ho);
			HalconCpp::HomMat2dTranslate(Ho, -10.0, -10.0, &Ho);
			HalconCpp::HImage offImg = bun.compositeOverlay(base, makeOpaqueRedRgba(32, 32), Ho, 255);
			HalconCpp::HTuple g0 = offImg.GetGrayval(0, 0);
			HalconCpp::HTuple g5 = offImg.GetGrayval(5, 5);
			std::cout << "offscreen (0,0)=(" << g0[0].I() << "," << g0[1].I() << "," << g0[2].I() << ")"
				<< " (5,5)=(" << g5[0].I() << "," << g5[1].I() << "," << g5[2].I() << ")"
				<< " expect red" << std::endl;
		}

		// 完全出界：应原样返回底图
		{
			HalconCpp::HTuple Hf;
			HalconCpp::HomMat2dIdentity(&Hf);
			HalconCpp::HomMat2dTranslate(Hf, 500.0, 500.0, &Hf);
			HalconCpp::HImage farImg = bun.compositeOverlay(base, makeOpaqueRedRgba(32, 32), Hf, 255);
			std::cout << "far-away channels=" << farImg.CountChannels().I()
				<< " (expect 1 = 原样返回)" << std::endl;
		}

		// 旋转 90°：套版四角包围盒 + 仿射仍应落在平移位置附近
		{
			HalconCpp::HTuple Hr;
			HalconCpp::HomMat2dIdentity(&Hr);
			HalconCpp::HomMat2dRotate(Hr, 3.14159265358979323846 / 2.0, 0.0, 0.0, &Hr);
			HalconCpp::HomMat2dTranslate(Hr, 72.0, 40.0, &Hr);  // 旋转后 (row,col)->(-col,row)，平移补偿
			HalconCpp::HImage rot = bun.compositeOverlay(base, makeOpaqueRedRgba(32, 32), Hr, 255);
			HalconCpp::HTuple g = rot.GetGrayval(56, 56);
			std::cout << "rot90 (56,56)=(" << g[0].I() << "," << g[1].I() << "," << g[2].I() << ")"
				<< " expect red" << std::endl;
		}

		// 耗时基准：生产尺寸 1500x1024，套版 210x294，连续 100 次
		{
			HalconCpp::HImage bigBase;
			HalconCpp::GenImageConst(&bigBase, "byte", 1500, 1024);
			HalconCpp::HTuple Hb;
			HalconCpp::HomMat2dIdentity(&Hb);
			HalconCpp::HomMat2dTranslate(Hb, 700.0, 500.0, &Hb);
			const auto t0 = std::chrono::steady_clock::now();
			for (int i = 0; i < 100; ++i)
				bun.compositeOverlay(bigBase, makeOpaqueRedRgba(210, 294), Hb, 180);
			const auto t1 = std::chrono::steady_clock::now();
			std::cout << "perf 1500x1024 + 210x294 stamp: "
				<< std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() / 100.0 / 1000.0
				<< " ms/次" << std::endl;
		}
		return 0;
	}

	test::StampPatternBunTest::runBasicTest();

	return 0;
}
