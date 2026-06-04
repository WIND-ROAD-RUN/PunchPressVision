#pragma once
#include <string>
#include "halconcpp/HalconCpp.h"
namespace inf
{
	namespace Config
	{
		class NinePointCfg
		{
			//TODO:完成畸变矫正的序列化和反序列化以及参数


			//九点标定的参数
			//测量矩形的参数
			double MeasureLength1 = 100;
			double MeasureLength2 = 50;
			double MeasureThreshold = 1;
			double num_Measure = 5;

		//九点标定换算的矩阵
			HalconCpp::HTuple outHomMat2D ;

			//标定板参数
		
			int xnumber = 7;
			int ynumber = 7;
			double distance = 0.007;
			double scale = 0.5;
			/**7 :x方向圆点数量
				* 7 : y方向圆点数量
				* 0.007 两个圆之间的距离， 单位米
				* 0.5 比例值： Mark直径比上Mark中心距离
				* caltab.descr 描述文件，用于标定文件的生成
				* caltab.ps 标定文件，用于生产标定板*/
				//gen_caltab(7, 7, 0.007, 0.5, 'D:/caltab.descr', 'D:/caltab.ps')

			




		public:
			void saveInDir(const std::string& dirPath);
			void loadInDir(const std::string& dirPath);
		};
	}
}
