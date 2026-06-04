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

    public:
        std::vector<ShapeModelInfo> getAllModelInfos() const;
        bool getModelInfo(int index, ShapeModelInfo& outInfo) const;
        int getModelCount() const;

        int createModel(const std::string& name, ShapeModelType type);
        bool updateModelInfo(int index, const ShapeModelInfo& info);
        bool deleteModel(int index);

        bool loadModelData(int index, ShapeModelData& outData) const;
        bool saveModelData(int index, const ShapeModelData& data);

        std::string getModelFolderPath(int index) const;

        bool loadDefaultShape(ShapeModelData& outData) const;
        bool setDefaultShape(int index);
        int getDefaultShapeIndex() const;

    private:
        std::vector<ShapeModelInfo> modelInfos_;
        int defaultShapeIndex_ = -1;
        std::string rootPath_;

    private:
        std::string generateDateFolderName() const;
        bool saveIndexToFolder(const ShapeModelInfo& info) const;
        bool loadIndexFromFolder(const std::string& folderPath, ShapeModelInfo& outInfo);
        bool scanModelFolders();
        bool saveDefaultShapeMarker() const;
        bool loadDefaultShapeMarker();
    };
}
