#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

#include <filesystem>
#include <fstream>
#include <json/json.h>

namespace inf
{
	namespace Config
	{
		namespace
		{
			namespace fs = std::filesystem;

			constexpr const char* kModelInfoFile = "model_info.json";

			void replaceFile(const fs::path& tmp, const fs::path& target)
			{
				if (fs::exists(target))
				{
					try { fs::remove(target); }
					catch (...) {}
				}
				fs::rename(tmp, target);
			}

			bool readJsonSafe(const fs::path& filePath, Json::Value& root)
			{
				if (!fs::exists(filePath))
					return false;
				std::ifstream ifs(filePath);
				if (!ifs)
					return false;

				Json::CharReaderBuilder builder;
				builder["collectComments"] = false;
				std::string errs;
				if (!Json::parseFromStream(builder, ifs, &root, &errs))
					return false;
				return true;
			}

			void writeJsonSafe(const fs::path& filePath, const Json::Value& root)
			{
				fs::create_directories(filePath.parent_path());
				fs::path tmp = filePath;
				tmp += ".tmp";

				std::ofstream ofs(tmp);
				if (!ofs)
					return;

				Json::StreamWriterBuilder builder;
				builder["indentation"] = "  ";
				std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
				writer->write(root, &ofs);
				ofs << '\n';
				ofs.close();

				replaceFile(tmp, filePath);
			}
		}

		void ShapeModelData::loadInDir(const std::string& dir)
		{
		}

		void ShapeModelData::saveInDir(const std::string& dir)
		{
		}

		void ShapeModelInfo::loadInDir(const std::string& dir)
		{
			try
			{
				const fs::path dirPath(dir);
				folder_path_ = dirPath.string();

				Json::Value root;
				if (!readJsonSafe(dirPath / kModelInfoFile, root))
					return;

				id_ = root.get("id", id_).asString();
				name = root.get("name", name).asString();
				create_time_ = root.get("createTime", create_time_).asString();
				update_time_ = root.get("updateTime", update_time_).asString();
				folder_path_ = root.get("folderPath", folder_path_).asString();
			}
			catch (...)
			{
				// Ignore load errors
			}
		}

		void ShapeModelInfo::saveInDir(const std::string& dir)
		{
			try
			{
				const fs::path dirPath(dir);

				Json::Value root;
				root["id"] = id_;
				root["name"] = name;
				root["createTime"] = create_time_;
				root["updateTime"] = update_time_;
				root["folderPath"] = folder_path_.empty() ? dirPath.string() : folder_path_;

				writeJsonSafe(dirPath / kModelInfoFile, root);
			}
			catch (...)
			{
				// Ignore save errors
			}
		}

		void ShapeModelItem::loadInDir(const std::string& dir)
		{
			info.loadInDir(dir);
			data.loadInDir(dir);
		}

		void ShapeModelItem::saveInDir(const std::string& dir)
		{
			info.saveInDir(dir);
			data.saveInDir(dir);
		}
	}
}