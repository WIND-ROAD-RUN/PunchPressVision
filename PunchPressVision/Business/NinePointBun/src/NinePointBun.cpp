#include "Business/NinePointBun/NinePointBun.hpp"

namespace bun
{
	NinePointBun::NinePointBun(inf::infrastructure& inf)
		: inf_(inf)
	{
	}

	void NinePointBun::calculateNinePointConfig()
	{
		Config::NinePointCfg ninePointCfg;

		//TODO:

		inf_.nine_point_module_->ninePointConfig = ninePointCfg;
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

	void NinePointBun::build()
	{
	}

	void NinePointBun::destroy()
	{
	}

	void NinePointBun::start()
	{
	}

	void NinePointBun::stop()
	{
	}
}
