#include "ShapeModelManagerModule.t.hpp"

#include <iostream>

#include"infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

namespace test
{
    void ShapeModelManagerModuleTest::runBasicTest()
    {
        inf::ShapeModelManagerModule manager;
        manager.build();

        inf::Config::ShapeModelInfo info;
		
        info.loadInDir(R"(C:\Users\zfkjCompile\Desktop\1)");
        manager.destroy();
    }
}

int main()
{
    test::ShapeModelManagerModuleTest::runBasicTest();
    return 0;
}
