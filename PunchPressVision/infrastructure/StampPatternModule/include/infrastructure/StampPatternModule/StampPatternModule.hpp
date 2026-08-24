#pragma once

#include <memory>
#include <string>
#include <vector>

#include "global/GlobalInterface.hpp"
#include "infrastructure/StampPatternModule/StampPatternModuleTypes.hpp"
#include "infrastructure/StampPatternModule/Config/StampPatternItem.hpp"

namespace inf
{
    // 套版（StampPattern）管理器：独立图库的增删改查。
    // 套版不参与定位计算，仅作为"要冲的图案外形"参考图叠加显示，辅助定义/验证中心点。
    // 与 ShapeModelManagerModule 并列：套版是独立图库，模型可后续通过套版 id 引用。
    class StampPatternModule
        : public global::IInfrastructure
    {
    public:
        StampPatternModule();
        ~StampPatternModule();
    private:
        std::vector<Config::StampPatternInfo> stamp_pattern_infos_{};
    public:
        const std::vector<Config::StampPatternInfo>& getStampPatternInfos() const { return stamp_pattern_infos_; }
    public:
        void readAllStampPatternInfos();
    public:
        // 增：导入套版文件，返回生成的 info；失败返回空 info。
        // 支持普通图片（原始字节复制，保留透明通道）与 CAD DXF 图纸（渲染为 RGBA 图片）。
        Config::StampPatternInfo importStampPattern(const std::string& sourceImagePath, const std::string& name);
        // 删
        void deleteStampPattern(const std::string& id);
        // 查：单个（含图片数据）
        Config::StampPatternItem getStampPatternItem(const std::string& id) const;
        // 改：对齐参数 / 透明度等数据（不改动图片文件）
        void changeStampPattern(const std::string& id, const Config::StampPatternData& data);
        // 改：重命名
        void changeStampPattern(const std::string& id, const std::string& newName);
    public:
        static std::string getCurrentTime_yyMMddHHmmsszzz();
    public:
        void build() override;
        void destroy() override;
    };
}
