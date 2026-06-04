#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace inf
{
    enum class ShapeModelType
    {
        Unknown,
        Circle,
        Rectangle,
        Polygon,
        Custom
    };

    struct ShapeModelInfo
    {
        std::string id;
        std::string name;
        ShapeModelType type = ShapeModelType::Unknown;
        std::string createTime;
        std::string updateTime;
        std::string folderPath;
        std::unordered_map<std::string, std::string> customFields;
    };

    struct ShapeModelData
    {
        // TODO: 用户应在此定义具体的Shape模型数据结构
        std::vector<std::uint8_t> rawData;
    };
}
