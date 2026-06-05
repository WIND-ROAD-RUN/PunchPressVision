#pragma once

#include <memory>
#include <string>
#include <vector>

#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModuleTypes.hpp"
#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

namespace inf
{
    class ShapeModelManagerModule
    {
    public:
        ShapeModelManagerModule();
        ~ShapeModelManagerModule();
    public:
        std::vector<Config::ShapeModelInfo> shape_model_infos{};
    public:
        void readAllShapeModelInfos();
    public:
        void addShapeModelItem(const Config::ShapeModelData & data,Config::ShapeModelInfo info);
        void deleteShapeModelItem(const std::string& id);
    public:
        static std::string getCurrentTime_yyMMddHHmmsszzz();
    public:
        void build();
        void destroy();
    };
}
