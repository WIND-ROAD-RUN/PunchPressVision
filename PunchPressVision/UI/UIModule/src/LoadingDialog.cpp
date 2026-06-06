#include "LoadingDialog.h"

#include <QVBoxLayout>

#include "ui_LoadingDialog.h"

LoadingDialog::LoadingDialog(QWidget* parent)
	: QDialog(parent) {
	setWindowTitle("加载中...");
	setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint); // 禁用关闭按钮
	setModal(true);

	QVBoxLayout* layout = new QVBoxLayout(this);
	label = new QLabel("正在加载...", this);
	progressBar = new QProgressBar(this);
	progressBar->setRange(0, 0); // 设置为不确定模式

	layout->addWidget(label);
	layout->addWidget(progressBar);
	setLayout(layout);
}

void LoadingDialog::updateMessage(const QString& message)
{
	label->setText(message);
}