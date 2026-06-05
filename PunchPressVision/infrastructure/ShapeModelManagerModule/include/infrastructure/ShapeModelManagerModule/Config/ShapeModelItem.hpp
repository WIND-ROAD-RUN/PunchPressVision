#pragma once
#include <string>

namespace Config
{
	struct ShapeModelData
	{
		//TODO:完成模型数据的加载和保存
	public:
		void loadInDir(const std::string& dir);
		void saveInDir(const std::string& dir);
	};



	struct ShapeModelInfo
	{
	public:
		struct BaseInfo
		{
			std::string name;
		};
	public:
		BaseInfo base_info;
	private:
		std::string id_;
		std::string create_time_;
		std::string update_time_;
		std::string folder_path_;
	public:
		std::string getId() const { return id_; }
		std::string getCreateTime() const { return create_time_; }
		std::string getUpdateTime() const { return update_time_; }
		std::string getFolderPath() const { return folder_path_; }
		void setId(const std::string& id) { this->id_ = id; }
		void setCreateTime(const std::string& createTime) { this->create_time_ = createTime; }
		void setUpdateTime(const std::string& updateTime) { this->update_time_ = updateTime; }
		void setFolderPath(const std::string& folderPath) { this->folder_path_ = folderPath; }
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