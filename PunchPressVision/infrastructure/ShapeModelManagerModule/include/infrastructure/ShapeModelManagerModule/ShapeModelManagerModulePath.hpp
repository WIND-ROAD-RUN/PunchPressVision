#pragma once

#include <string>
#include "global/GlobalPath.hpp"

namespace inf
{
    inline struct
    {
    public:
        std::string RootPath = global::ProjectRootPath + "/ShapeModels/";
    } ShapeModelManagerModulePath;
}
