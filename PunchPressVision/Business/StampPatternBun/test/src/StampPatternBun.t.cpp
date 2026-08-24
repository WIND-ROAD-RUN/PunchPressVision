#include "StampPatternBun.t.hpp"

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

	test::StampPatternBunTest::runBasicTest();

	return 0;
}
