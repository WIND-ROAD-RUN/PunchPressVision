#pragma once

#include"rwul/oso/oso_core.h"


namespace Config {
    class cameraCfg
    {
    public:
        cameraCfg() = default;
        ~cameraCfg() = default;

        cameraCfg(const rw::oso::ObjectStoreAssembly& assembly);
        cameraCfg(const cameraCfg& obj);

        cameraCfg& operator=(const cameraCfg& obj);
        operator rw::oso::ObjectStoreAssembly() const;
        bool operator==(const cameraCfg& obj) const;
        bool operator!=(const cameraCfg& obj) const;

    public:
        int exposureTime1{ 200 };
        int exposureTime2{ 200 };
        int gain1{ 1 };
        int gain2{ 1 };
    };

    inline cameraCfg::cameraCfg(const rw::oso::ObjectStoreAssembly& assembly)
    {
        auto isAccountAssembly = assembly.getName();
        if (isAccountAssembly != "$class$cameraCfg$")
        {
            throw std::runtime_error("Assembly is not $class$cameraCfg$");
        }
        auto exposureTime1Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$exposureTime1$"));
        if (!exposureTime1Item) {
            throw std::runtime_error("$variable$exposureTime1 is not found");
        }
        exposureTime1 = exposureTime1Item->getValueAsInt();
        auto exposureTime2Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$exposureTime2$"));
        if (!exposureTime2Item) {
            throw std::runtime_error("$variable$exposureTime2 is not found");
        }
        exposureTime2 = exposureTime2Item->getValueAsInt();
        auto gain1Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$gain1$"));
        if (!gain1Item) {
            throw std::runtime_error("$variable$gain1 is not found");
        }
        gain1 = gain1Item->getValueAsInt();
        auto gain2Item = rw::oso::ObjectStoreCoreToItem(assembly.getItem("$variable$gain2$"));
        if (!gain2Item) {
            throw std::runtime_error("$variable$gain2 is not found");
        }
        gain2 = gain2Item->getValueAsInt();
    }

    inline cameraCfg::cameraCfg(const cameraCfg& obj)
    {
        exposureTime1 = obj.exposureTime1;
        exposureTime2 = obj.exposureTime2;
        gain1 = obj.gain1;
        gain2 = obj.gain2;
    }

    inline cameraCfg& cameraCfg::operator=(const cameraCfg& obj)
    {
        if (this != &obj) {
            exposureTime1 = obj.exposureTime1;
            exposureTime2 = obj.exposureTime2;
            gain1 = obj.gain1;
            gain2 = obj.gain2;
        }
        return *this;
    }

    inline cameraCfg::operator rw::oso::ObjectStoreAssembly() const
    {
        rw::oso::ObjectStoreAssembly assembly;
        assembly.setName("$class$cameraCfg$");
        auto exposureTime1Item = std::make_shared<rw::oso::ObjectStoreItem>();
        exposureTime1Item->setName("$variable$exposureTime1$");
        exposureTime1Item->setValueFromInt(exposureTime1);
        assembly.addItem(exposureTime1Item);
        auto exposureTime2Item = std::make_shared<rw::oso::ObjectStoreItem>();
        exposureTime2Item->setName("$variable$exposureTime2$");
        exposureTime2Item->setValueFromInt(exposureTime2);
        assembly.addItem(exposureTime2Item);
        auto gain1Item = std::make_shared<rw::oso::ObjectStoreItem>();
        gain1Item->setName("$variable$gain1$");
        gain1Item->setValueFromInt(gain1);
        assembly.addItem(gain1Item);
        auto gain2Item = std::make_shared<rw::oso::ObjectStoreItem>();
        gain2Item->setName("$variable$gain2$");
        gain2Item->setValueFromInt(gain2);
        assembly.addItem(gain2Item);
        return assembly;
    }

    inline bool cameraCfg::operator==(const cameraCfg& obj) const
    {
        return exposureTime1 == obj.exposureTime1 && exposureTime2 == obj.exposureTime2 && gain1 == obj.gain1 && gain2 == obj.gain2;
    }

    inline bool cameraCfg::operator!=(const cameraCfg& obj) const
    {
        return !(*this == obj);
    }

}

