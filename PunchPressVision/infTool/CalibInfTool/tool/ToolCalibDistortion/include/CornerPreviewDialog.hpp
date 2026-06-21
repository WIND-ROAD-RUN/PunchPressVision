#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include <vector>

#include "halconcpp/HalconCpp.h"

namespace ui { class HalconInteractiveLabel; }
namespace infTool { class CalibInfTool; }
namespace Config { class CalibConfigItem; }

/**
 * @brief Halcon 标定板标记点预览对话框。
 *
 * 对每一张标定图像调用 CalibInfTool::drawCalibMarks()，
 * 使用 HalconInteractiveLabel 渲染图像并叠加 XLD 标记点，
 * 支持鼠标滚轮缩放与拖拽平移。
 */
class CornerPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CornerPreviewDialog(infTool::CalibInfTool& calibTool,
        Config::CalibConfigItem& cfg,
        const std::vector<HalconCpp::HImage>& images,
        QWidget* parent = nullptr);

    ~CornerPreviewDialog() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onPrev();
    void onNext();

private:
    void updateView();

private:
    infTool::CalibInfTool& calibTool_;
    Config::CalibConfigItem& cfg_;
    const std::vector<HalconCpp::HImage>& images_;
    size_t currentIndex_ = 0;

    ui::HalconInteractiveLabel* view_ = nullptr;
    QLabel* counterLabel_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
};
