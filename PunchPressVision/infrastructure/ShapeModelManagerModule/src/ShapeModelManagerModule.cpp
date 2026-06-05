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
        auto currentTime = getCurrentTime_yyMMddHHmmss();
        info.setCreateTime(currentTime);
        info.setUpdateTime(currentTime);
        info.setFolderPath(ShapeModelManagerModulePath.RootPath+ currentTime);
        info.setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + "_" + currentTime);

		Config::ShapeModelItem item;
		item.info = info;
		item.data = data;

        item.saveInDir(info.getFolderPath());
        readAllShapeModelInfos();
    }

    std::string ShapeModelManagerModule::getCurrentTime_yyMMddHHmmss()
    {
        return QDateTime::currentDateTime().toString("yyyyMMddHHmmss").toStdString();
    }

    void ShapeModelManagerModule::build()
    {

    }

    void ShapeModelManagerModule::destroy()
    {
        // TODO: 保存所有待持久化的数据
    }

}
