#include "Business/NinePointBun/NinePointBun.hpp"

#include <chrono>

#include "infrastructure/ControlModule/ControlModule.hpp"

namespace bun
{
	NinePointBun::NinePointBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	NinePointBun::~NinePointBun()
	{
		stopAutoCalibration();
	}

	void NinePointBun::calculateNinePointConfig()
	{
		// 触发自动化九点标定流程（PLC 协同），结果写入并持久化到 NinePointModule。
		// 不再用空配置覆盖现有标定结果。
		std::string err;
		startAutoCalibration(&err);
	}

	bool NinePointBun::calcPixToWorldHomMat2D(const std::vector<Point2D>& pixPoints,
		const std::vector<Point2D>& worldPoints,
		Config::NinePointCfg& ninePointCfg)
	{
		ninePointCfg.outHomMat2D = HalconCpp::HTuple();

		if (pixPoints.size() < 3 || pixPoints.size() != worldPoints.size())
			return false;

		HalconCpp::HTuple pixRows, pixCols;
		HalconCpp::HTuple worldRows, worldCols;

		// Halcon: row=y, col=x
		for (size_t i = 0; i < pixPoints.size(); ++i)
		{
			pixRows.Append(static_cast<double>(pixPoints[i].y));
			pixCols.Append(static_cast<double>(pixPoints[i].x));

			worldRows.Append(static_cast<double>(worldPoints[i].y));
			worldCols.Append(static_cast<double>(worldPoints[i].x));
		}

		try
		{
			// 最小二乘拟合（多点）
			HalconCpp::VectorToHomMat2d(pixRows, pixCols, worldRows, worldCols, &ninePointCfg.outHomMat2D);
			return ninePointCfg.outHomMat2D.TupleLength() > 0;
		}
		catch (...)
		{
			ninePointCfg.outHomMat2D = HalconCpp::HTuple();
			return false;
		}
	}

	bool NinePointBun::pixToWorld(const HalconCpp::HTuple& homMat2D,
		double pixX, double pixY,
		double& outWorldX, double& outWorldY)
	{
		outWorldX = 0.0;
		outWorldY = 0.0;

		// hom_mat2d 典型长度为 6
		if (homMat2D.TupleLength() < 6)
			return false;

		try
		{
			// Halcon: row=y, col=x
			HalconCpp::HTuple worldRow, worldCol;
			HalconCpp::AffineTransPoint2d(homMat2D,
				static_cast<double>(pixY),
				static_cast<double>(pixX),
				&worldRow,
				&worldCol);

			if (worldRow.TupleLength() < 1 || worldCol.TupleLength() < 1)
				return false;

			// 世界: x=col, y=row
			outWorldX = worldCol[0].D();
			outWorldY = worldRow[0].D();
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void NinePointBun::setMechanicalCoords(const std::vector<Point2D>& coords)
	{
		mechanicalCoords_ = coords;
	}

	bool NinePointBun::startAutoCalibration(std::string* errorMsg)
	{
		if (worker_.joinable())
		{
			if (errorMsg) *errorMsg = "九点标定已在进行中";
			return false;
		}
		if (mechanicalCoords_.size() < 3)
		{
			if (errorMsg) *errorMsg = "九点机械坐标数量不足（至少 3 个）";
			return false;
		}
		stopRequested_.store(false, std::memory_order_release);
		worker_ = std::thread([this] { runAutoCalibration(); });
		return true;
	}

	void NinePointBun::stopAutoCalibration()
	{
		stopRequested_.store(true, std::memory_order_release);
		if (worker_.joinable())
			worker_.join();
	}

	void NinePointBun::runAutoCalibration()
	{
		try
		{
			const int total = static_cast<int>(mechanicalCoords_.size());
			std::vector<Point2D> pixPoints;
			std::vector<Point2D> worldPoints;

			auto* control = inf_.control_module_ ? inf_.control_module_.get() : nullptr;

			for (int i = 0; i < total; ++i)
			{
				if (stopRequested_.load(std::memory_order_acquire))
				{
					emit calibrationFinished(false, QStringLiteral("九点标定已取消"));
					return;
				}

				emit calibrationProgress(i + 1, total);

				const Point2D mech = mechanicalCoords_[i];

				// 1. 写入机械坐标到 PLC（40103 起，X/Y 交替）并写当前点位序号（40102）
				if (control && control->isConnected())
				{
					// TODO(硬件): 确认浮点坐标的寄存器编排方式。
					control->writeFloat(40103 + i * 2, static_cast<float>(mech.x));
					control->writeFloat(40103 + i * 2 + 2, static_cast<float>(mech.y));
					control->writeRegister(40102, static_cast<uint16_t>(i + 1));
				}

				// 2. 轮询到位确认（40101）
				bool arrived = !control || !control->isConnected(); // 无硬件时直接放行（便于离线流程）
				for (int retry = 0; retry < 300 && !arrived; ++retry)
				{
					if (stopRequested_.load(std::memory_order_acquire))
					{
						emit calibrationFinished(false, QStringLiteral("九点标定已取消"));
						return;
					}
					uint16_t status = 0;
					if (control && control->readRegister(40101, status) && status == 1)
					{
						arrived = true;
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				if (!arrived)
				{
					emit calibrationFinished(false, QStringLiteral("PLC 到位超时"));
					return;
				}

				// 3. 采集图像并检测标定标记得到像素坐标
				// TODO(硬件): 采集单帧并用 Halcon 检测标记点像素坐标。
				//   rw::hoec::MatInfo frame;
				//   inf_.camera_module_->captureSingleFrame(global::CameraIndex::Camera1, frame);
				//   ...检测得到 pix...
				Point2D pix{ 0.0, 0.0 };

				pixPoints.push_back(pix);
				worldPoints.push_back(mech);
			}

			// 4. 计算变换矩阵
			Config::NinePointCfg cfg = inf_.nine_point_module_->ninePointConfig;
			if (!calcPixToWorldHomMat2D(pixPoints, worldPoints, cfg))
			{
				emit calibrationFinished(false, QStringLiteral("变换矩阵计算失败"));
				return;
			}

			// 5. 保存
			inf_.nine_point_module_->ninePointConfig = cfg;
			inf_.nine_point_module_->save();

			emit calibrationFinished(true, QStringLiteral("九点标定完成"));
		}
		catch (...)
		{
			emit calibrationFinished(false, QStringLiteral("九点标定异常"));
		}
	}

	void NinePointBun::build()
	{
	}

	void NinePointBun::destroy()
	{
		stopAutoCalibration();
	}

	void NinePointBun::start()
	{
	}

	void NinePointBun::stop()
	{
		stopAutoCalibration();
	}
}
