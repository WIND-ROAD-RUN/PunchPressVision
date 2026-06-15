#include "CornerPreviewDialog.hpp"

#include "infTool/CalibInfTool/CalibInfTool.hpp"
#include "infrastructure/CalibConfigModule/Config/CalibConfig.hpp"

#include <QHBoxLayout>
#include <QMessageBox>
#include <QResizeEvent>
#include <QVBoxLayout>

CornerPreviewDialog::CornerPreviewDialog(
    infTool::CalibInfTool& calibTool,
    Config::CalibConfig& cfg,
    const std::vector<HalconCpp::HImage>& images,
    QWidget* parent)
    : QDialog(parent)
    , calibTool_(calibTool)
    , cfg_(cfg)
    , images_(images)
{
    setWindowTitle(QStringLiteral("Halcon 标定板标记点预览"));
    resize(800, 600);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(8, 8, 8, 8);
    rootLayout->setSpacing(8);

    // Halcon 渲染宿主
    viewHost_ = new QWidget(this);
    viewHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    viewHost_->setMinimumSize(640, 480);
    viewHost_->setStyleSheet(QStringLiteral("background-color: #333;"));
    rootLayout->addWidget(viewHost_, 1);

    displayView_.ensure(viewHost_);

    // 计数器
    counterLabel_ = new QLabel(this);
    counterLabel_->setAlignment(Qt::AlignCenter);
    rootLayout->addWidget(counterLabel_);

    // 导航按钮
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

CornerPreviewDialog::~CornerPreviewDialog()
{
    displayView_.close();
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
    if (currentIndex_ + 1 < images_.size())
    {
        ++currentIndex_;
        updateView();
    }
}

void CornerPreviewDialog::updateView()
{
    const size_t total = images_.size();
    if (total == 0)
    {
        counterLabel_->setText(QStringLiteral("0 / 0"));
        prevBtn_->setEnabled(false);
        nextBtn_->setEnabled(false);
        return;
    }

    if (currentIndex_ >= total)
        currentIndex_ = total - 1;

    counterLabel_->setText(
        QStringLiteral("%1 / %2").arg(currentIndex_ + 1).arg(total));

    prevBtn_->setEnabled(currentIndex_ > 0);
    nextBtn_->setEnabled(currentIndex_ + 1 < total);

    refreshDisplay();
}

void CornerPreviewDialog::refreshDisplay()
{
    const auto& img = images_[currentIndex_];
    if (!img.IsInitialized())
        return;

    // 显示原始图像
    displayView_.display(img);

    // 尝试查找标定板标记并叠加 XLD
    bool isOk = false;
    HalconCpp::HObject marksXld, marksRegion;
    std::string err;
    calibTool_.drawCalibMarks(img, cfg_, isOk, marksXld, marksRegion, &err);

    if (isOk)
    {
        displayView_.overlayXld(marksXld);
        setWindowTitle(
            QStringLiteral("Halcon 标定板标记点预览 — %1 / %2 (检测成功)")
                .arg(currentIndex_ + 1).arg(images_.size()));
    }
    else
    {
        setWindowTitle(
            QStringLiteral("Halcon 标定板标记点预览 — %1 / %2 (未检测到标定板)")
                .arg(currentIndex_ + 1).arg(images_.size()));
    }
}

void CornerPreviewDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    displayView_.resizeToHost();
}
