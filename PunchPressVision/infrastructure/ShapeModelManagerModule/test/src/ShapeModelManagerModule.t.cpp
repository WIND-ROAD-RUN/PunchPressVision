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
        inf::Config::ShapeModelInfo info;
		info.name = "TestModel";
        manager.addShapeModelItem(data, info);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        info.name = "TestModel1";
        manager.addShapeModelItem(data, info);

        manager.destroy();
    }
}

int main()
{
    test::ShapeModelManagerModuleTest::runBasicTest();
    return 0;
}
