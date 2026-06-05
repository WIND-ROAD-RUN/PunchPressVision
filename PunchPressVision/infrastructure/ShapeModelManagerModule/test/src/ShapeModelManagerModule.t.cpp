#include "ShapeModelManagerModule.t.hpp"

#include <iostream>

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

        manager.destroy();
    }
}

int main()
{
    test::ShapeModelManagerModuleTest::runBasicTest();
    return 0;
}
