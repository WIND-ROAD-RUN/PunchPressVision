#include "TwoCameraSpliceModule.t.hpp"

#include "infrastructure/TwoCameraSpliceModule/Config/TwoCameraSpliceConfig.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>

using namespace Config;
namespace fs = std::filesystem;

static int g_failures = 0;
#define CHECK(cond) do { \
	if (!(cond)) { std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++g_failures; } \
	else { std::printf("OK:   %s\n", #cond); } } while (0)

// 生成指定尺寸的灰底白块图（按宽度区分图像来源）
static HalconCpp::HObject makeImage(Hlong w, Hlong h)
{
	HalconCpp::HImage img("byte", w, h);
	HalconCpp::HObject region;
	HalconCpp::GenRectangle1(&region, 2, 2, h - 3, w - 3);
	HalconCpp::HImage painted;
	HalconCpp::PaintRegion(region, img, &painted, 255, "fill");
	return painted;
}

static Hlong imageWidth(const HalconCpp::HObject& img)
{
	HalconCpp::HTuple w, h;
	HalconCpp::GetImageSize(img, &w, &h);
	return w;
}

static void writeTextFile(const fs::path& p, const std::string& content)
{
	std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
	ofs << content;
}

static int countTmpLikeFiles(const fs::path& dir)
{
	int n = 0;
	for (const auto& e : fs::directory_iterator(dir))
		if (e.is_regular_file()
			&& e.path().filename().string().find(".tmp") != std::string::npos)
		{
			std::printf("      residue: %s\n", e.path().filename().string().c_str());
			++n;
		}
	return n;
}

int main()
{
	const fs::path dir = fs::temp_directory_path() / "ppv_splice_cfg_test";
	fs::remove_all(dir);

	// ============================================================
	// 场景1：正常保存（连存两次），文件名必须精确、无任何临时残留
	// ============================================================
	std::printf("---- scenario 1: save twice, exact filenames ----\n");
	{
		TwoCameraSpliceCfg cfg;
		cfg.camera1Piccture = makeImage(16, 16);
		cfg.camera2Piccture = makeImage(16, 16);
		cfg.MapSingle1 = makeImage(8, 8);
		cfg.MapSingle2 = makeImage(8, 8);
		cfg.caltabDescrPath = "C:/calib/plate.descr";
		cfg.camera1Gain = 1.5; cfg.camera1Exposure = 8000;
		cfg.camera2Gain = 2.5; cfg.camera2Exposure = 9000;
		cfg.DiffHeight = 3.3; cfg.OverlapInPercent = 70;
		cfg.DistancePlates = 12.5; cfg.pixTowWorld = 0.00123;
		cfg.rectifiedWidth = 4096; cfg.rectifiedHeight = 1024;

		cfg.saveInDir(dir.string());
		cfg.saveInDir(dir.string());  // 第二次保存：覆盖路径

		CHECK(fs::exists(dir / "camera1_picture.bmp"));
		CHECK(fs::exists(dir / "camera2_picture.bmp"));
		CHECK(fs::exists(dir / "map_single1.hobj"));
		CHECK(fs::exists(dir / "map_single2.hobj"));
		CHECK(fs::exists(dir / "two_camera_splice_params.txt"));
		CHECK(countTmpLikeFiles(dir) == 0);
	}

	// ============================================================
	// 场景2：完整加载回读（并触发 backup/ 生成）
	// ============================================================
	std::printf("---- scenario 2: load back ----\n");
	{
		TwoCameraSpliceCfg cfg;
		cfg.loadInDir(dir.string());
		CHECK(cfg.MapSingle1.IsInitialized());
		CHECK(cfg.MapSingle2.IsInitialized());
		CHECK(cfg.camera1Piccture.IsInitialized());
		CHECK(cfg.pixTowWorld == 0.00123);
		CHECK(cfg.camera1Gain == 1.5);
		CHECK(cfg.camera2Exposure == 9000);
		CHECK(cfg.rectifiedWidth == 4096);
		CHECK(cfg.caltabDescrPath == "C:/calib/plate.descr");
		CHECK(fs::exists(dir / "backup" / "map_single1.hobj"));
		CHECK(countTmpLikeFiles(dir / "backup") == 0);
	}

	// ============================================================
	// 场景3：模拟现场残留（旧版本 bug 产物），验证调和收养/清理
	//   camera1: 正式文件缺失，残留 x.bmp.tmp.bmp（有效，宽32）→ 收养
	//   camera2: 正式文件缺失，残留 x.tmp.bmp（有效，宽48）    → 收养
	//   map1:    正式文件缺失，残留 x.hobj.tmp.hobj（有效）    → 收养
	//   map2:    正式文件缺失，残留 x.hobj.tmp（损坏）         → 删除
	//   params:  正式文件缺失，残留 x.txt.tmp（有效）          → 收养
	// ============================================================
	std::printf("---- scenario 3: reconcile legacy garbage ----\n");
	{
		fs::remove_all(dir);
		fs::create_directories(dir);
		HalconCpp::HImage(makeImage(32, 32)).WriteImage("bmp", 0,
			(dir / "camera1_picture.bmp.tmp.bmp").string().c_str());
		HalconCpp::HImage(makeImage(48, 48)).WriteImage("bmp", 0,
			(dir / "camera2_picture.tmp.bmp").string().c_str());
		HalconCpp::WriteObject(makeImage(8, 8),
			(dir / "map_single1.hobj.tmp.hobj").string().c_str());
		writeTextFile(dir / "map_single2.hobj.tmp", "corrupted-not-a-hobj");
		writeTextFile(dir / "two_camera_splice_params.txt.tmp",
			"caltabDescrPath=D:/field/plate.descr\n"
			"camera1Gain=9.9\n"
			"pixTowWorld=0.00777\n"
			"rectifiedWidth=2048\n"
			"rectifiedHeight=512\n");

		TwoCameraSpliceCfg cfg;
		cfg.loadInDir(dir.string());

		// 有效残留被收养为正式文件并读出
		CHECK(cfg.camera1Piccture.IsInitialized());
		CHECK(imageWidth(cfg.camera1Piccture) == 32);
		CHECK(cfg.camera2Piccture.IsInitialized());
		CHECK(imageWidth(cfg.camera2Piccture) == 48);
		CHECK(cfg.MapSingle1.IsInitialized());
		CHECK(cfg.pixTowWorld == 0.00777);
		CHECK(cfg.camera1Gain == 9.9);
		CHECK(cfg.rectifiedWidth == 2048);
		CHECK(cfg.caltabDescrPath == "D:/field/plate.descr");
		// 正式文件落盘
		CHECK(fs::exists(dir / "camera1_picture.bmp"));
		CHECK(fs::exists(dir / "map_single1.hobj"));
		CHECK(fs::exists(dir / "two_camera_splice_params.txt"));
		// 损坏残留被删除、无其他垃圾
		CHECK(!fs::exists(dir / "map_single2.hobj.tmp"));
		CHECK(countTmpLikeFiles(dir) == 0);
	}

	std::printf(g_failures == 0 ? "ALL PASSED\n" : "%d FAILURES\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
