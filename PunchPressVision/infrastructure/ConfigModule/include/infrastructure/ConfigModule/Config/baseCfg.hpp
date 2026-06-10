#pragma once

#include"rwul/oso/oso_core.h"


namespace Config {
    class BaseCfg
    {
    public:
        BaseCfg() = default;
        ~BaseCfg() = default;

        BaseCfg(const rw::oso::ObjectStoreAssembly& assembly);
        BaseCfg(const BaseCfg& obj);

        BaseCfg& operator=(const BaseCfg& obj);
        operator rw::oso::ObjectStoreAssembly() const;
        bool operator==(const BaseCfg& obj) const;
        bool operator!=(const BaseCfg& obj) const;

    public:
        std::string cameraIp1{ "192.168.11.1" };
        std::string cameraIp2{ "192.168.11.2" };
        std::string plcIp{ "192.168.11.10" };
        int plcPort{ 502 };
    };

    inline BaseCfg::BaseCfg(const rw::oso::ObjectStoreAssembly& assembly)
    {
        auto isAccountAssembly = assembly.getName();
        if (isAccountAssembly != "$class$BaseCfg$")
        {
            throw std::runtime_error("Assembly is not $class$BaseCfg$");
        }
        auto cameraIp1Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$cameraIp1$"));
        if (!cameraIp1Item) {
            throw std::runtime_error("$variable$cameraIp1 is not found");
        }
        cameraIp1 = cameraIp1Item->getValueAsString();
        auto cameraIp2Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$cameraIp2$"));
        if (!cameraIp2Item) {
            throw std::runtime_error("$variable$cameraIp2 is not found");
        }
        cameraIp2 = cameraIp2Item->getValueAsString();
        auto plcIpItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$plcIp$"));
        if (!plcIpItem) {
            throw std::runtime_error("$variable$plcIp is not found");
        }
        plcIp = plcIpItem->getValueAsString();
        auto plcPortItem = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$plcPort$"));
        if (!plcPortItem) {
            throw std::runtime_error("$variable$plcPort is not found");
        }
        plcPort = plcPortItem->getValueAsInt();
    }

    inline BaseCfg::BaseCfg(const BaseCfg& obj)
    {
        cameraIp1 = obj.cameraIp1;
        cameraIp2 = obj.cameraIp2;
        plcIp = obj.plcIp;
        plcPort = obj.plcPort;
    }

    inline BaseCfg& BaseCfg::operator=(const BaseCfg& obj)
    {
        if (this != &obj) {
            cameraIp1 = obj.cameraIp1;
            cameraIp2 = obj.cameraIp2;
            plcIp = obj.plcIp;
            plcPort = obj.plcPort;
        }
        return *this;
    }

    inline BaseCfg::operator rw::oso::ObjectStoreAssembly() const
    {
        rw::oso::ObjectStoreAssembly assembly;
        assembly.setName("$class$BaseCfg$");
        auto cameraIp1Item = std::make_shared<rw::oso::ObjectStoreItem>();
        cameraIp1Item->setName("$variable$cameraIp1$");
        cameraIp1Item->setValueFromString(cameraIp1);
        assembly.addItem(cameraIp1Item);
        auto cameraIp2Item = std::make_shared<rw::oso::ObjectStoreItem>();
        cameraIp2Item->setName("$variable$cameraIp2$");
        cameraIp2Item->setValueFromString(cameraIp2);
        assembly.addItem(cameraIp2Item);
        auto plcIpItem = std::make_shared<rw::oso::ObjectStoreItem>();
        plcIpItem->setName("$variable$plcIp$");
        plcIpItem->setValueFromString(plcIp);
        assembly.addItem(plcIpItem);
        auto plcPortItem = std::make_shared<rw::oso::ObjectStoreItem>();
        plcPortItem->setName("$variable$plcPort$");
        plcPortItem->setValueFromInt(plcPort);
        assembly.addItem(plcPortItem);
        return assembly;
    }

    inline bool BaseCfg::operator==(const BaseCfg& obj) const
    {
        return cameraIp1 == obj.cameraIp1 && cameraIp2 == obj.cameraIp2 && plcIp == obj.plcIp && plcPort == obj.plcPort;
    }

    inline bool BaseCfg::operator!=(const BaseCfg& obj) const
    {
        return !(*this == obj);
    }

}

