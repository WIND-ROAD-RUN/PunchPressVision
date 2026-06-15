#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

class OpenCvCalibrator;

/**
 * @brief 棋盘格角点预览对话框。
 *
 * 接收 OpenCvCalibrator 实例，浏览所有成功检测到角点的图像，
 * 并在图像上叠加绘制角点标记。
 */
class CornerPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CornerPreviewDialog(const OpenCvCalibrator& calibrator, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onPrev();
    void onNext();

private:
    void updateView();
    void refreshImageLabel();

private:
    const OpenCvCalibrator& calibrator_;
    size_t currentIndex_ = 0;

    QLabel* imageLabel_ = nullptr;
    QLabel* counterLabel_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
};
