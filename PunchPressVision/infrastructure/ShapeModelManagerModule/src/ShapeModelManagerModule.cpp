#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModule.hpp"
#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModulePath.hpp"

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

    void ShapeModelManagerModule::build()
    {
        rootPath_ = ShapeModelManagerModulePath.RootPath;

        // TODO: 创建根目录（如果不存在）
        // std::filesystem::create_directories(rootPath_);

        // TODO: 扫描所有日期命名的文件夹并加载索引
        scanModelFolders();

        // TODO: 加载默认Shape标记
        loadDefaultShapeMarker();
    }

    void ShapeModelManagerModule::destroy()
    {
        // TODO: 保存所有待持久化的数据
    }

    std::vector<ShapeModelInfo> ShapeModelManagerModule::getAllModelInfos() const
    {
        return modelInfos_;
    }

    bool ShapeModelManagerModule::getModelInfo(int index, ShapeModelInfo& outInfo) const
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }
        outInfo = modelInfos_[index];
        return true;
    }

    int ShapeModelManagerModule::getModelCount() const
    {
        return static_cast<int>(modelInfos_.size());
    }

    int ShapeModelManagerModule::createModel(const std::string& name, ShapeModelType type)
    {
        // TODO: 生成日期命名的文件夹
        // std::string folderName = generateDateFolderName();
        // std::string folderPath = rootPath_ + folderName;

        // TODO: 创建文件夹
        // std::filesystem::create_directories(folderPath);

        ShapeModelInfo info;
        info.name = name;
        info.type = type;
        // TODO: 根据type设置typeName
        // TODO: 设置createTime, updateTime, folderPath, id

        // TODO: 保存索引文件到文件夹
        // saveIndexToFolder(info);

        // TODO: 添加到modelInfos_并返回索引
        // modelInfos_.push_back(info);
        // return static_cast<int>(modelInfos_.size()) - 1;

        return -1; // TODO: 替换为实际实现
    }

    bool ShapeModelManagerModule::updateModelInfo(int index, const ShapeModelInfo& info)
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }

        // TODO: 更新updateTime
        // TODO: 保存索引文件到文件夹
        // saveIndexToFolder(info);

        modelInfos_[index] = info;
        return true;
    }

    bool ShapeModelManagerModule::deleteModel(int index)
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }

        // TODO: 删除对应的文件夹及其内容
        // std::filesystem::remove_all(modelInfos_[index].folderPath);

        // TODO: 如果删除的是默认Shape，需要清除默认标记

        modelInfos_.erase(modelInfos_.begin() + index);

        // TODO: 更新defaultShapeIndex_
        return true;
    }

    bool ShapeModelManagerModule::loadModelData(int index, ShapeModelData& outData) const
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }

        // TODO: 从模型文件夹加载具体的Shape模型数据
        // 用户可在此实现自定义的加载逻辑
        // std::string folderPath = modelInfos_[index].folderPath;

        return false; // TODO: 替换为实际实现
    }

    bool ShapeModelManagerModule::saveModelData(int index, const ShapeModelData& data)
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }

        // TODO: 将Shape模型数据保存到模型文件夹
        // 用户可在此实现自定义的存储逻辑
        // std::string folderPath = modelInfos_[index].folderPath;

        return false; // TODO: 替换为实际实现
    }

    std::string ShapeModelManagerModule::getModelFolderPath(int index) const
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return {};
        }
        return modelInfos_[index].folderPath;
    }

    bool ShapeModelManagerModule::loadDefaultShape(ShapeModelData& outData) const
    {
        if (defaultShapeIndex_ < 0 || defaultShapeIndex_ >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }
        return loadModelData(defaultShapeIndex_, outData);
    }

    bool ShapeModelManagerModule::setDefaultShape(int index)
    {
        if (index < 0 || index >= static_cast<int>(modelInfos_.size()))
        {
            return false;
        }
        defaultShapeIndex_ = index;
        return saveDefaultShapeMarker();
    }

    int ShapeModelManagerModule::getDefaultShapeIndex() const
    {
        return defaultShapeIndex_;
    }

    std::string ShapeModelManagerModule::generateDateFolderName() const
    {
        // TODO: 生成基于当前日期时间的文件夹名称，如 YYYYMMDD_HHMMSS
        // 如果同一秒内已有文件夹，建议追加序号避免冲突
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
        localtime_s(&tm, &time);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    bool ShapeModelManagerModule::saveIndexToFolder(const ShapeModelInfo& info) const
    {
        // TODO: 将ShapeModelInfo序列化并保存到 info.folderPath + IndexFileName
        return false;
    }

    bool ShapeModelManagerModule::loadIndexFromFolder(const std::string& folderPath, ShapeModelInfo& outInfo)
    {
        // TODO: 从 folderPath + IndexFileName 反序列化 ShapeModelInfo
        return false;
    }

    bool ShapeModelManagerModule::scanModelFolders()
    {
        // TODO: 遍历 rootPath_ 下的所有日期命名文件夹
        // 对每个文件夹调用 loadIndexFromFolder 加载模型信息
        // 按日期或用户自定义规则排序后存入 modelInfos_
        return true;
    }

    bool ShapeModelManagerModule::saveDefaultShapeMarker() const
    {
        // TODO: 将 defaultShapeIndex_ 对应的模型ID写入 rootPath_ + DefaultShapeMarker
        return false;
    }

    bool ShapeModelManagerModule::loadDefaultShapeMarker()
    {
        // TODO: 从 rootPath_ + DefaultShapeMarker 读取默认模型ID
        // 并在 modelInfos_ 中查找对应的索引设置到 defaultShapeIndex_
        return true;
    }
}
