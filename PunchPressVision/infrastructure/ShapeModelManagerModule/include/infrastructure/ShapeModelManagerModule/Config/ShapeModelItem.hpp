#pragma once
#include <string>

namespace inf
{
	namespace Config
	{
        struct ShapeModelData
        {
			//TODO:完成模型数据的加载和保存
        public:
            void loadInDir(const std::string & dir);
            void saveInDir(const std::string & dir);
        };


        struct ShapeModelInfo
        {
        public:
            std::string id;
            std::string name;
            std::string createTime;
            std::string updateTime;
            std::string folderPath;
        public:
            void loadInDir(const std::string& dir);
            void saveInDir(const std::string& dir);
        };

        struct ShapeModelItem
        {
        public:
			ShapeModelInfo info;
        public:
			ShapeModelData data;
        public:
            void loadInDir(const std::string& dir);
            void saveInDir(const std::string& dir);
        };


        
	}
}
