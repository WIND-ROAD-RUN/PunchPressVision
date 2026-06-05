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
				folderPath = dirPath.string();

				Json::Value root;
				if (!readJsonSafe(dirPath / kModelInfoFile, root))
					return;

				id = root.get("id", id).asString();
				name = root.get("name", name).asString();
				createTime = root.get("createTime", createTime).asString();
				updateTime = root.get("updateTime", updateTime).asString();
				folderPath = root.get("folderPath", folderPath).asString();
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
				root["id"] = id;
				root["name"] = name;
				root["createTime"] = createTime;
				root["updateTime"] = updateTime;
				root["folderPath"] = folderPath.empty() ? dirPath.string() : folderPath;

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