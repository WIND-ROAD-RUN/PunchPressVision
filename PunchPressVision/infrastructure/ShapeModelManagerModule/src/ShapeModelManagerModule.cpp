#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModule.hpp"
#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModulePath.hpp"

#include <QDateTime>
#include <QUuid>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace inf
{
    ShapeModelManagerModule::ShapeModelManagerModule()
    {
    }

    ShapeModelManagerModule::~ShapeModelManagerModule()
    {
        destroy();
    }

    void ShapeModelManagerModule::readAllShapeModelInfos()
    {
        shape_model_infos.clear();
        try
        {
            namespace fs = std::filesystem;
            const fs::path root(ShapeModelManagerModulePath.RootPath);
            if (!fs::exists(root) || !fs::is_directory(root))
                return;

            for (const auto& entry : fs::directory_iterator(root))
            {
                if (!entry.is_directory())
                    continue;

                Config::ShapeModelInfo info;
                info.loadInDir(entry.path().string());
                shape_model_infos.push_back(std::move(info));
            }
        }
        catch (...)
        {
            // Ignore load errors
        }
    }

    void ShapeModelManagerModule::addShapeModelItem(const Config::ShapeModelData& data, Config::ShapeModelInfo info)
    {
        auto currentTime = getCurrentTime_yyMMddHHmmsszzz();
        info.setCreateTime(currentTime);
        info.setUpdateTime(currentTime);
        info.setFolderPath(ShapeModelManagerModulePath.RootPath+ currentTime);
        info.setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + "_" + currentTime);

		Config::ShapeModelItem item;
		item.info = info;
		item.data = data;

        item.saveInDir(info.getFolderPath());
        shape_model_infos.push_back(std::move(info));
    }

    void ShapeModelManagerModule::deleteShapeModelItem(const std::string& id)
    {
        try
        {
            namespace fs = std::filesystem;

            const auto it = std::find_if(shape_model_infos.begin(), shape_model_infos.end(),
                [&id](const Config::ShapeModelInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == shape_model_infos.end())
                return;

            const fs::path dirPath(it->getFolderPath());
            if (!dirPath.empty() && fs::exists(dirPath))
            {
                fs::remove_all(dirPath);
            }

            shape_model_infos.erase(it);
        }
        catch (...)
        {
            // Ignore delete errors
        }
    }

    std::string ShapeModelManagerModule::getCurrentTime_yyMMddHHmmsszzz()
    {
        return QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz").toStdString();
    }

    void ShapeModelManagerModule::build()
    {
        readAllShapeModelInfos();
    }

    void ShapeModelManagerModule::destroy()
    {
        // TODO: 保存所有待持久化的数据
    }

}
