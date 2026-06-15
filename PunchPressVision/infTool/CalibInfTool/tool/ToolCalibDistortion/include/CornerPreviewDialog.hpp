#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include <vector>

#include "halconcpp/HalconCpp.h"

#include "DisplayView.hpp"

namespace infTool { class CalibInfTool; }
namespace Config { class CalibConfigItem; }

/**
 * @brief Halcon 标定板标记点预览对话框。
 *
 * 对每一张标定图像调用 CalibInfTool::drawCalibMarks()，
 * 使用 Halcon 窗口渲染图像并叠加 XLD 标记点。
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
    void refreshDisplay();

private:
    infTool::CalibInfTool& calibTool_;
    Config::CalibConfigItem& cfg_;
    const std::vector<HalconCpp::HImage>& images_;
    size_t currentIndex_ = 0;

    QWidget* viewHost_ = nullptr;
    QLabel* counterLabel_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;

    DisplayView displayView_;
};
