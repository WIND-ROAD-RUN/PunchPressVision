#pragma once

#include <string>
#include "global/GlobalPath.hpp"

namespace inf
{
    inline struct
    {
    public:
        std::string RootPath = global::ProjectRootPath + "/ShapeModels/";
        std::string IndexFileName = "model_index.xml";
        std::string DefaultShapeMarker = "default_shape.txt";
    } ShapeModelManagerModulePath;
}
