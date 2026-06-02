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
    }

    inline BaseCfg::BaseCfg(const BaseCfg& obj)
    {
        cameraIp1 = obj.cameraIp1;
    }

    inline BaseCfg& BaseCfg::operator=(const BaseCfg& obj)
    {
        if (this != &obj) {
            cameraIp1 = obj.cameraIp1;
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
        return assembly;
    }

    inline bool BaseCfg::operator==(const BaseCfg& obj) const
    {
        return cameraIp1 == obj.cameraIp1;
    }

    inline bool BaseCfg::operator!=(const BaseCfg& obj) const
    {
        return !(*this == obj);
    }

}

