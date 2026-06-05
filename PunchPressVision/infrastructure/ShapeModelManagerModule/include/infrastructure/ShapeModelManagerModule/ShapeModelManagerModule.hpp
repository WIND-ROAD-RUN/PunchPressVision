#pragma once

#include <memory>
#include <string>
#include <vector>

#include "infrastructure/ShapeModelManagerModule/ShapeModelManagerModuleTypes.hpp"

namespace inf
{
    class ShapeModelManagerModule
    {
    public:
        ShapeModelManagerModule();
        ~ShapeModelManagerModule();

        void build();
        void destroy();
    };
}
