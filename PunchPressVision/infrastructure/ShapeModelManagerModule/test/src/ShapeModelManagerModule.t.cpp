#include "ShapeModelManagerModule.t.hpp"

#include <iostream>
#include <thread>

#include"infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

namespace test
{
    void ShapeModelManagerModuleTest::runBasicTest()
    {
        inf::ShapeModelManagerModule manager;
        manager.build();

        inf::Config::ShapeModelData data;
        inf::Config::ShapeModelInfo::BaseInfo baseInfo;
        baseInfo.name = "TestModel";
        manager.addShapeModelItem(data, baseInfo);
        std::this_thread::sleep_for(std::chrono::seconds(1));

        baseInfo.name = "TestModel1";
        auto info = manager.addShapeModelItem(data, baseInfo);

        manager.deleteShapeModelItem(info.getId());

        manager.destroy();
    }
}

int main()
{
    test::ShapeModelManagerModuleTest::runBasicTest();
    return 0;
}
