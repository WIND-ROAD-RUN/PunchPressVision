#pragma once

#include"rwul/oso/oso_core.h"


namespace Config {
    class PlcAddressCfg
    {
    public:
        PlcAddressCfg() = default;
        ~PlcAddressCfg() = default;

        PlcAddressCfg(const rw::oso::ObjectStoreAssembly& assembly);
        PlcAddressCfg(const PlcAddressCfg& obj);

        PlcAddressCfg& operator=(const PlcAddressCfg& obj);
        operator rw::oso::ObjectStoreAssembly() const;
        bool operator==(const PlcAddressCfg& obj) const;
        bool operator!=(const PlcAddressCfg& obj) const;

    public:
        int regOffsetX{ 40001 };
        int regOffsetY{ 40003 };
        int regAngle{ 40005 };
        int regValid{ 40007 };
        int regNinePointArrived{ 40101 };
        int regNinePointIndex{ 40102 };
        int regNinePointCoordsStart{ 40103 };
    };

    inline PlcAddressCfg::PlcAddressCfg(const rw::oso::ObjectStoreAssembly& assembly)
    {
        auto isAccountAssembly = assembly.getName();
        if (isAccountAssembly != "$class$PlcAddressCfg$")
        {
            throw std::runtime_error("Assembly is not $class$PlcAddressCfg$");
        }
        auto regOffsetXItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regOffsetX$"));
        if (!regOffsetXItem) {
            throw std::runtime_error("$variable$regOffsetX is not found");
        }
        regOffsetX = regOffsetXItem->getValueAsInt();
        auto regOffsetYItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regOffsetY$"));
        if (!regOffsetYItem) {
            throw std::runtime_error("$variable$regOffsetY is not found");
        }
        regOffsetY = regOffsetYItem->getValueAsInt();
        auto regAngleItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regAngle$"));
        if (!regAngleItem) {
            throw std::runtime_error("$variable$regAngle is not found");
        }
        regAngle = regAngleItem->getValueAsInt();
        auto regValidItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regValid$"));
        if (!regValidItem) {
            throw std::runtime_error("$variable$regValid is not found");
        }
        regValid = regValidItem->getValueAsInt();
        auto regNinePointArrivedItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regNinePointArrived$"));
        if (!regNinePointArrivedItem) {
            throw std::runtime_error("$variable$regNinePointArrived is not found");
        }
        regNinePointArrived = regNinePointArrivedItem->getValueAsInt();
        auto regNinePointIndexItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regNinePointIndex$"));
        if (!regNinePointIndexItem) {
            throw std::runtime_error("$variable$regNinePointIndex is not found");
        }
        regNinePointIndex = regNinePointIndexItem->getValueAsInt();
        auto regNinePointCoordsStartItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$regNinePointCoordsStart$"));
        if (!regNinePointCoordsStartItem) {
            throw std::runtime_error("$variable$regNinePointCoordsStart is not found");
        }
        regNinePointCoordsStart = regNinePointCoordsStartItem->getValueAsInt();
    }

    inline PlcAddressCfg::PlcAddressCfg(const PlcAddressCfg& obj)
    {
        regOffsetX = obj.regOffsetX;
        regOffsetY = obj.regOffsetY;
        regAngle = obj.regAngle;
        regValid = obj.regValid;
        regNinePointArrived = obj.regNinePointArrived;
        regNinePointIndex = obj.regNinePointIndex;
        regNinePointCoordsStart = obj.regNinePointCoordsStart;
    }

    inline PlcAddressCfg& PlcAddressCfg::operator=(const PlcAddressCfg& obj)
    {
        if (this != &obj) {
            regOffsetX = obj.regOffsetX;
            regOffsetY = obj.regOffsetY;
            regAngle = obj.regAngle;
            regValid = obj.regValid;
            regNinePointArrived = obj.regNinePointArrived;
            regNinePointIndex = obj.regNinePointIndex;
            regNinePointCoordsStart = obj.regNinePointCoordsStart;
        }
        return *this;
    }

    inline PlcAddressCfg::operator rw::oso::ObjectStoreAssembly() const
    {
        rw::oso::ObjectStoreAssembly assembly;
        assembly.setName("$class$PlcAddressCfg$");
        auto regOffsetXItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regOffsetXItem->setName("$variable$regOffsetX$");
        regOffsetXItem->setValueFromInt(regOffsetX);
        assembly.addItem(regOffsetXItem);
        auto regOffsetYItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regOffsetYItem->setName("$variable$regOffsetY$");
        regOffsetYItem->setValueFromInt(regOffsetY);
        assembly.addItem(regOffsetYItem);
        auto regAngleItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regAngleItem->setName("$variable$regAngle$");
        regAngleItem->setValueFromInt(regAngle);
        assembly.addItem(regAngleItem);
        auto regValidItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regValidItem->setName("$variable$regValid$");
        regValidItem->setValueFromInt(regValid);
        assembly.addItem(regValidItem);
        auto regNinePointArrivedItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regNinePointArrivedItem->setName("$variable$regNinePointArrived$");
        regNinePointArrivedItem->setValueFromInt(regNinePointArrived);
        assembly.addItem(regNinePointArrivedItem);
        auto regNinePointIndexItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regNinePointIndexItem->setName("$variable$regNinePointIndex$");
        regNinePointIndexItem->setValueFromInt(regNinePointIndex);
        assembly.addItem(regNinePointIndexItem);
        auto regNinePointCoordsStartItem = std::make_shared<rw::oso::ObjectStoreItem>();
        regNinePointCoordsStartItem->setName("$variable$regNinePointCoordsStart$");
        regNinePointCoordsStartItem->setValueFromInt(regNinePointCoordsStart);
        assembly.addItem(regNinePointCoordsStartItem);
        return assembly;
    }

    inline bool PlcAddressCfg::operator==(const PlcAddressCfg& obj) const
    {
        return regOffsetX == obj.regOffsetX && regOffsetY == obj.regOffsetY && regAngle == obj.regAngle && regValid == obj.regValid && regNinePointArrived == obj.regNinePointArrived && regNinePointIndex == obj.regNinePointIndex && regNinePointCoordsStart == obj.regNinePointCoordsStart;
    }

    inline bool PlcAddressCfg::operator!=(const PlcAddressCfg& obj) const
    {
        return !(*this == obj);
    }

}

