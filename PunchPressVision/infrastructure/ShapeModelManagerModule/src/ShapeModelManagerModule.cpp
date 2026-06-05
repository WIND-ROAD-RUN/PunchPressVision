#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModule.hpp"
#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModulePath.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace inf
{
    ShapeModelManagerModule::ShapeModelManagerModule()
    {
    }

    ShapeModelManagerModule::~ShapeModelManagerModule()
    {
        destroy();
    }

    void ShapeModelManagerModule::build()
    {

    }

    void ShapeModelManagerModule::destroy()
    {
        // TODO: 保存所有待持久化的数据
    }

}
