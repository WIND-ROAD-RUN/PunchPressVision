#include "ShapeModelManagerModule.t.hpp"

#include <iostream>

namespace test
{
    void ShapeModelManagerModuleTest::runBasicTest()
    {
        inf::ShapeModelManagerModule manager;
        manager.build();

        // TODO: 添加测试用例

        manager.destroy();
    }
}

int main()
{
    test::ShapeModelManagerModuleTest::runBasicTest();
    return 0;
}
