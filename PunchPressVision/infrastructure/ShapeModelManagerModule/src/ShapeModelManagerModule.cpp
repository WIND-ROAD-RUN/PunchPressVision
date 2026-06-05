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
        shape_model_infos_.clear();
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
                shape_model_infos_.push_back(std::move(info));
            }
        }
        catch (...)
        {
            // Ignore load errors
        }
    }

    Config::ShapeModelInfo ShapeModelManagerModule::addShapeModelItem(const Config::ShapeModelData& data, Config::ShapeModelInfo::BaseInfo baseInfo)
    {
        Config::ShapeModelInfo info;
        auto currentTime = getCurrentTime_yyMMddHHmmsszzz();
        info.setCreateTime(currentTime);
        info.setUpdateTime(currentTime);
        info.setFolderPath(ShapeModelManagerModulePath.RootPath+ currentTime);
        info.setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + "_" + currentTime);
		info.base_info = std::move(baseInfo);

		Config::ShapeModelItem item;
		item.info = info;
		item.data = data;

        item.saveInDir(info.getFolderPath());
        shape_model_infos_.push_back(info);

        return info;
    }

    void ShapeModelManagerModule::deleteShapeModelItem(const std::string& id)
    {
        try
        {
            namespace fs = std::filesystem;

            const auto it = std::find_if(shape_model_infos_.begin(), shape_model_infos_.end(),
                [&id](const Config::ShapeModelInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == shape_model_infos_.end())
                return;

            const fs::path dirPath(it->getFolderPath());
            if (!dirPath.empty() && fs::exists(dirPath))
            {
                fs::remove_all(dirPath);
            }

            shape_model_infos_.erase(it);
        }
        catch (...)
        {
            // Ignore delete errors
        }
    }

    Config::ShapeModelItem ShapeModelManagerModule::getShapeModelItem(const std::string& id) const
    {
        Config::ShapeModelItem item;

        const auto it = std::find_if(shape_model_infos_.begin(), shape_model_infos_.end(),
            [&id](const Config::ShapeModelInfo& info)
            {
                return info.getId() == id;
            });

        if (it == shape_model_infos_.end())
            return item;

        item.loadInDir(it->getFolderPath());
        return item;
    }

    void ShapeModelManagerModule::changeShapeModelItem(const std::string& id, const Config::ShapeModelData& data)
    {
        try
        {
            const auto it = std::find_if(shape_model_infos_.begin(), shape_model_infos_.end(),
                [&id](const Config::ShapeModelInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == shape_model_infos_.end())
                return;

            Config::ShapeModelItem item;
            item.loadInDir(it->getFolderPath());
            item.data = data;
            item.info.setUpdateTime(getCurrentTime_yyMMddHHmmsszzz());

            item.saveInDir(item.info.getFolderPath());

            // 同步内存中的更新时间
            it->setUpdateTime(item.info.getUpdateTime());
        }
        catch (...)
        {
            // Ignore change errors
        }
    }

    void ShapeModelManagerModule::changeShapeModelItem(const std::string& id,
	    const Config::ShapeModelInfo::BaseInfo& baseInfo)
    {
        try
        {
            const auto it = std::find_if(shape_model_infos_.begin(), shape_model_infos_.end(),
                [&id](const Config::ShapeModelInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == shape_model_infos_.end())
                return;

            Config::ShapeModelItem item;
            item.loadInDir(it->getFolderPath());
            item.info.base_info = baseInfo;
            item.info.setUpdateTime(getCurrentTime_yyMMddHHmmsszzz());

            item.saveInDir(item.info.getFolderPath());

            // 同步内存中的信息
            it->base_info = baseInfo;
            it->setUpdateTime(item.info.getUpdateTime());
        }
        catch (...)
        {
            // Ignore change errors
        }
    }

    void ShapeModelManagerModule::changeShapeModelItem(const std::string& id, const Config::ShapeModelData& data,
	    const Config::ShapeModelInfo::BaseInfo& baseInfo)
    {
        try
        {
            const auto it = std::find_if(shape_model_infos_.begin(), shape_model_infos_.end(),
                [&id](const Config::ShapeModelInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == shape_model_infos_.end())
                return;

            Config::ShapeModelItem item;
            item.loadInDir(it->getFolderPath());
            item.data = data;
            item.info.base_info = baseInfo;
            item.info.setUpdateTime(getCurrentTime_yyMMddHHmmsszzz());

            item.saveInDir(item.info.getFolderPath());

            // 同步内存中的信息
            it->base_info = baseInfo;
            it->setUpdateTime(item.info.getUpdateTime());
        }
        catch (...)
        {
            // Ignore change errors
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
