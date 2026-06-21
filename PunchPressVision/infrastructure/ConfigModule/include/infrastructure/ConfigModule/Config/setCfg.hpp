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
    }

    inline SetCfg::SetCfg(const SetCfg& obj)
    {
        diffHeight = obj.diffHeight;
    }

    inline SetCfg& SetCfg::operator=(const SetCfg& obj)
    {
        if (this != &obj) {
            diffHeight = obj.diffHeight;
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
        return assembly;
    }

    inline bool SetCfg::operator==(const SetCfg& obj) const
    {
        return diffHeight == obj.diffHeight;
    }

    inline bool SetCfg::operator!=(const SetCfg& obj) const
    {
        return !(*this == obj);
    }

}

