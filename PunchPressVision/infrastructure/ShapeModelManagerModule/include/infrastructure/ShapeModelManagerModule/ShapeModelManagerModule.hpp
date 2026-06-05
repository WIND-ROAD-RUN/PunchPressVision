#pragma once

#include <memory>
#include <string>
#include <vector>

#include "global/GlobalInterface.hpp"
#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModuleTypes.hpp"
#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

namespace inf
{
    class ShapeModelManagerModule
        : public global::IInfrastructure
    {
    public:
        ShapeModelManagerModule();
        ~ShapeModelManagerModule();
    private:
        std::vector<Config::ShapeModelInfo> shape_model_infos_{};
    public:
		const std::vector<Config::ShapeModelInfo>& getShapeModelInfos() const { return shape_model_infos_; }
    public:
        void readAllShapeModelInfos();
    public:
        Config::ShapeModelInfo addShapeModelItem(const Config::ShapeModelData & data,Config::ShapeModelInfo::BaseInfo baseInfo);
        void deleteShapeModelItem(const std::string& id);
		Config::ShapeModelItem getShapeModelItem(const std::string& id) const;
        void changeShapeModelItem(const std::string& id, const Config::ShapeModelData& data);
        void changeShapeModelItem(const std::string& id, const Config::ShapeModelInfo::BaseInfo& baseInfo);
        void changeShapeModelItem(const std::string& id, const Config::ShapeModelData& data,const Config::ShapeModelInfo::BaseInfo& baseInfo);
    public:
        static std::string getCurrentTime_yyMMddHHmmsszzz();
    public:
        void build() override;
        void destroy() override;
    };
}
