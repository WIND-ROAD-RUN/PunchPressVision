#include "infrastructure/StampPatternModule/StampPatternModule.hpp"
#include "infrastructure/StampPatternModule/StampPatternModulePath.hpp"

#include <QDateTime>
#include <QUuid>

#include <algorithm>
#include <filesystem>

namespace inf
{
    StampPatternModule::StampPatternModule()
    {
    }

    StampPatternModule::~StampPatternModule()
    {
        destroy();
    }

    void StampPatternModule::readAllStampPatternInfos()
    {
        stamp_pattern_infos_.clear();
        try
        {
            namespace fs = std::filesystem;
            const fs::path root(StampPatternModulePath.RootPath);
            if (!fs::exists(root) || !fs::is_directory(root))
                return;

            for (const auto& entry : fs::directory_iterator(root))
            {
                if (!entry.is_directory())
                    continue;

                Config::StampPatternInfo info;
                info.loadInDir(entry.path().string());
                stamp_pattern_infos_.push_back(std::move(info));
            }

            // 按创建时间降序排列（最近的在最上面）
            std::sort(stamp_pattern_infos_.begin(), stamp_pattern_infos_.end(),
                [](const Config::StampPatternInfo& a, const Config::StampPatternInfo& b) {
                    return a.getCreateTime() > b.getCreateTime();
                });
        }
        catch (...)
        {
            // Ignore load errors
        }
    }

    Config::StampPatternInfo StampPatternModule::importStampPattern(const std::string& sourceImagePath, const std::string& name)
    {
        Config::StampPatternInfo info;
        try
        {
            namespace fs = std::filesystem;

            auto currentTime = getCurrentTime_yyMMddHHmmsszzz();
            info.setCreateTime(currentTime);
            info.setUpdateTime(currentTime);
            info.setFolderPath(StampPatternModulePath.RootPath + "/" + currentTime);
            info.setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + "_" + currentTime);
            info.base_info.name = name;

            Config::StampPatternItem item;
            item.info = info;
            // 先落 info + 默认参数（建立目录并写元数据）
            item.saveInDir(info.getFolderPath());

            // 原始复制用户图片（保留透明通道）；失败则回滚刚创建的目录
            if (!Config::copyImageFile(sourceImagePath, info.getFolderPath() + "/" + Config::kPatternImageFileName))
            {
                const fs::path dirPath(info.getFolderPath());
                if (fs::exists(dirPath))
                    fs::remove_all(dirPath);
                return Config::StampPatternInfo{};
            }

            stamp_pattern_infos_.insert(stamp_pattern_infos_.begin(), info);
        }
        catch (...)
        {
            // Ignore import errors
        }
        return info;
    }

    void StampPatternModule::deleteStampPattern(const std::string& id)
    {
        try
        {
            namespace fs = std::filesystem;

            const auto it = std::find_if(stamp_pattern_infos_.begin(), stamp_pattern_infos_.end(),
                [&id](const Config::StampPatternInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == stamp_pattern_infos_.end())
                return;

            const fs::path dirPath(it->getFolderPath());
            if (!dirPath.empty() && fs::exists(dirPath))
            {
                fs::remove_all(dirPath);
            }

            stamp_pattern_infos_.erase(it);
        }
        catch (...)
        {
            // Ignore delete errors
        }
    }

    Config::StampPatternItem StampPatternModule::getStampPatternItem(const std::string& id) const
    {
        Config::StampPatternItem item;

        const auto it = std::find_if(stamp_pattern_infos_.begin(), stamp_pattern_infos_.end(),
            [&id](const Config::StampPatternInfo& info)
            {
                return info.getId() == id;
            });

        if (it == stamp_pattern_infos_.end())
            return item;

        item.loadInDir(it->getFolderPath());
        return item;
    }

    void StampPatternModule::changeStampPattern(const std::string& id, const Config::StampPatternData& data)
    {
        try
        {
            const auto it = std::find_if(stamp_pattern_infos_.begin(), stamp_pattern_infos_.end(),
                [&id](const Config::StampPatternInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == stamp_pattern_infos_.end())
                return;

            Config::StampPatternItem item;
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

    void StampPatternModule::changeStampPattern(const std::string& id, const std::string& newName)
    {
        try
        {
            const auto it = std::find_if(stamp_pattern_infos_.begin(), stamp_pattern_infos_.end(),
                [&id](const Config::StampPatternInfo& info)
                {
                    return info.getId() == id;
                });

            if (it == stamp_pattern_infos_.end())
                return;

            Config::StampPatternItem item;
            item.loadInDir(it->getFolderPath());
            item.info.base_info.name = newName;
            item.info.setUpdateTime(getCurrentTime_yyMMddHHmmsszzz());

            item.saveInDir(item.info.getFolderPath());

            // 同步内存中的信息
            it->base_info.name = newName;
            it->setUpdateTime(item.info.getUpdateTime());
        }
        catch (...)
        {
            // Ignore change errors
        }
    }

    std::string StampPatternModule::getCurrentTime_yyMMddHHmmsszzz()
    {
        return QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz").toStdString();
    }

    void StampPatternModule::build()
    {
        readAllStampPatternInfos();
    }

    void StampPatternModule::destroy()
    {
        // 套版在 import/change 时即原子落盘，无需在此额外持久化。
        // 仅释放内存索引（folder 内文件已是权威数据源）。
        stamp_pattern_infos_.clear();
    }

}
