#pragma once

#include"rwul/oso/oso_core.h"


namespace Config {
    class SetCfg
    {
    public:
        SetCfg() = default;
        ~SetCfg() = default;

        SetCfg(const rw::oso::ObjectStoreAssembly& assembly);
        SetCfg(const SetCfg& obj);

        SetCfg& operator=(const SetCfg& obj);
        operator rw::oso::ObjectStoreAssembly() const;
        bool operator==(const SetCfg& obj) const;
        bool operator!=(const SetCfg& obj) const;

    public:
        double diffHeight{ 0 };
        bool matchRegionValid{ false };
        double matchRegionRow1{ 0 };
        double matchRegionCol1{ 0 };
        double matchRegionRow2{ 0 };
        double matchRegionCol2{ 0 };
    };

    inline SetCfg::SetCfg(const rw::oso::ObjectStoreAssembly& assembly)
    {
        auto isAccountAssembly = assembly.getName();
        if (isAccountAssembly != "$class$SetCfg$")
        {
            throw std::runtime_error("Assembly is not $class$SetCfg$");
        }
        auto diffHeightItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$diffHeight$"));
        if (!diffHeightItem) {
            throw std::runtime_error("$variable$diffHeight is not found");
        }
        diffHeight = diffHeightItem->getValueAsDouble();
        auto matchRegionValidItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$matchRegionValid$"));
        if (!matchRegionValidItem) {
            throw std::runtime_error("$variable$matchRegionValid is not found");
        }
        matchRegionValid = matchRegionValidItem->getValueAsBool();
        auto matchRegionRow1Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$matchRegionRow1$"));
        if (!matchRegionRow1Item) {
            throw std::runtime_error("$variable$matchRegionRow1 is not found");
        }
        matchRegionRow1 = matchRegionRow1Item->getValueAsDouble();
        auto matchRegionCol1Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$matchRegionCol1$"));
        if (!matchRegionCol1Item) {
            throw std::runtime_error("$variable$matchRegionCol1 is not found");
        }
        matchRegionCol1 = matchRegionCol1Item->getValueAsDouble();
        auto matchRegionRow2Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$matchRegionRow2$"));
        if (!matchRegionRow2Item) {
            throw std::runtime_error("$variable$matchRegionRow2 is not found");
        }
        matchRegionRow2 = matchRegionRow2Item->getValueAsDouble();
        auto matchRegionCol2Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$matchRegionCol2$"));
        if (!matchRegionCol2Item) {
            throw std::runtime_error("$variable$matchRegionCol2 is not found");
        }
        matchRegionCol2 = matchRegionCol2Item->getValueAsDouble();
    }

    inline SetCfg::SetCfg(const SetCfg& obj)
    {
        diffHeight = obj.diffHeight;
        matchRegionValid = obj.matchRegionValid;
        matchRegionRow1 = obj.matchRegionRow1;
        matchRegionCol1 = obj.matchRegionCol1;
        matchRegionRow2 = obj.matchRegionRow2;
        matchRegionCol2 = obj.matchRegionCol2;
    }

    inline SetCfg& SetCfg::operator=(const SetCfg& obj)
    {
        if (this != &obj) {
            diffHeight = obj.diffHeight;
            matchRegionValid = obj.matchRegionValid;
            matchRegionRow1 = obj.matchRegionRow1;
            matchRegionCol1 = obj.matchRegionCol1;
            matchRegionRow2 = obj.matchRegionRow2;
            matchRegionCol2 = obj.matchRegionCol2;
        }
        return *this;
    }

    inline SetCfg::operator rw::oso::ObjectStoreAssembly() const
    {
        rw::oso::ObjectStoreAssembly assembly;
        assembly.setName("$class$SetCfg$");
        auto diffHeightItem = std::make_shared<rw::oso::ObjectStoreItem>();
        diffHeightItem->setName("$variable$diffHeight$");
        diffHeightItem->setValueFromDouble(diffHeight);
        assembly.addItem(diffHeightItem);
        auto matchRegionValidItem = std::make_shared<rw::oso::ObjectStoreItem>();
        matchRegionValidItem->setName("$variable$matchRegionValid$");
        matchRegionValidItem->setValueFromBool(matchRegionValid);
        assembly.addItem(matchRegionValidItem);
        auto matchRegionRow1Item = std::make_shared<rw::oso::ObjectStoreItem>();
        matchRegionRow1Item->setName("$variable$matchRegionRow1$");
        matchRegionRow1Item->setValueFromDouble(matchRegionRow1);
        assembly.addItem(matchRegionRow1Item);
        auto matchRegionCol1Item = std::make_shared<rw::oso::ObjectStoreItem>();
        matchRegionCol1Item->setName("$variable$matchRegionCol1$");
        matchRegionCol1Item->setValueFromDouble(matchRegionCol1);
        assembly.addItem(matchRegionCol1Item);
        auto matchRegionRow2Item = std::make_shared<rw::oso::ObjectStoreItem>();
        matchRegionRow2Item->setName("$variable$matchRegionRow2$");
        matchRegionRow2Item->setValueFromDouble(matchRegionRow2);
        assembly.addItem(matchRegionRow2Item);
        auto matchRegionCol2Item = std::make_shared<rw::oso::ObjectStoreItem>();
        matchRegionCol2Item->setName("$variable$matchRegionCol2$");
        matchRegionCol2Item->setValueFromDouble(matchRegionCol2);
        assembly.addItem(matchRegionCol2Item);
        return assembly;
    }

    inline bool SetCfg::operator==(const SetCfg& obj) const
    {
        return diffHeight == obj.diffHeight && matchRegionValid == obj.matchRegionValid && matchRegionRow1 == obj.matchRegionRow1 && matchRegionCol1 == obj.matchRegionCol1 && matchRegionRow2 == obj.matchRegionRow2 && matchRegionCol2 == obj.matchRegionCol2;
    }

    inline bool SetCfg::operator!=(const SetCfg& obj) const
    {
        return !(*this == obj);
    }

}

