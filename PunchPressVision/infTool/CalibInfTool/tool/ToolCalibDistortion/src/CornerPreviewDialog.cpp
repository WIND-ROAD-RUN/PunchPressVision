#include "CornerPreviewDialog.hpp"

#include "OpenCvCalibrator.hpp"

#include <QHBoxLayout>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>

// 将 cv::Mat 转换为可在 QLabel 中显示的 QImage
static QImage cvMatToQImage(const cv::Mat& mat)
{
    if (mat.empty())
        return QImage();

    switch (mat.type())
    {
    case CV_8UC1:
    {
        return QImage(mat.data, mat.cols, mat.rows,
            static_cast<int>(mat.step), QImage::Format_Grayscale8).copy();
    }
    case CV_8UC3:
    {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage(rgb.data, rgb.cols, rgb.rows,
            static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    }
    case CV_8UC4:
    {
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        return QImage(rgba.data, rgba.cols, rgba.rows,
            static_cast<int>(rgba.step), QImage::Format_RGBA8888).copy();
    }
    default:
        return QImage();
    }
}

CornerPreviewDialog::CornerPreviewDialog(const OpenCvCalibrator& calibrator, QWidget* parent)
    : QDialog(parent)
    , calibrator_(calibrator)
{
    setWindowTitle(QStringLiteral("棋盘格角点预览"));
    resize(800, 600);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    imageLabel_ = new QLabel(this);
    imageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setStyleSheet(QStringLiteral("background-color: #333;"));
    imageLabel_->setMinimumSize(640, 480);
    rootLayout->addWidget(imageLabel_, 1);

    counterLabel_ = new QLabel(this);
    counterLabel_->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(counterLabel_);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    prevBtn_ = new QPushButton(QStringLiteral("上一张"), this);
    nextBtn_ = new QPushButton(QStringLiteral("下一张"), this);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), this);

    btnLayout->addWidget(prevBtn_);
    btnLayout->addWidget(nextBtn_);
    btnLayout->addWidget(closeBtn);
    btnLayout->addStretch();

    rootLayout->addLayout(btnLayout);

    connect(prevBtn_, &QPushButton::clicked, this, &CornerPreviewDialog::onPrev);
    connect(nextBtn_, &QPushButton::clicked, this, &CornerPreviewDialog::onNext);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    updateView();
}

void CornerPreviewDialog::onPrev()
{
    if (currentIndex_ > 0)
    {
        --currentIndex_;
        updateView();
    }
}

void CornerPreviewDialog::onNext()
{
    if (currentIndex_ + 1 < calibrator_.cornerImageCount())
    {
        ++currentIndex_;
        updateView();
    }
}

void CornerPreviewDialog::updateView()
{
    const size_t total = calibrator_.cornerImageCount();
    if (total == 0)
    {
        imageLabel_->setText(QStringLiteral("没有可用的角点预览图"));
        counterLabel_->setText(QStringLiteral("0 / 0"));
        prevBtn_->setEnabled(false);
        nextBtn_->setEnabled(false);
        return;
    }

    if (currentIndex_ >= total)
        currentIndex_ = total - 1;

    counterLabel_->setText(QStringLiteral("%1 / %2")
        .arg(currentIndex_ + 1).arg(total));

    prevBtn_->setEnabled(currentIndex_ > 0);
    nextBtn_->setEnabled(currentIndex_ + 1 < total);

    refreshImageLabel();
}

void CornerPreviewDialog::refreshImageLabel()
{
    cv::Mat img = calibrator_.getCornerImage(currentIndex_);
    if (img.empty())
    {
        imageLabel_->setText(QStringLiteral("无法加载该预览图"));
        return;
    }

    QImage qimg = cvMatToQImage(img);
    if (qimg.isNull())
    {
        imageLabel_->setText(QStringLiteral("图像格式不支持"));
        return;
    }

    const QSize viewSize = imageLabel_->size();
    if (viewSize.width() > 0 && viewSize.height() > 0)
        imageLabel_->setPixmap(QPixmap::fromImage(qimg).scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        imageLabel_->setPixmap(QPixmap::fromImage(qimg));
}

void CornerPreviewDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    refreshImageLabel();
}
