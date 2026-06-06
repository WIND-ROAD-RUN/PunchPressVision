#include "PictureViewerUtilty.h"

#include <complex>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

#include "ui_PictureViewerUtilty.h"

PictureViewerUtilty::PictureViewerUtilty(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::PictureViewerUtiltyClass())
{
	ui->setupUi(this);

	build_ui();
	build_connect();
}

PictureViewerUtilty::~PictureViewerUtilty()
{
	delete ui;
}

void PictureViewerUtilty::build_ui()
{
}

void PictureViewerUtilty::build_connect()
{
	connect(ui->pbtn_exit, &QPushButton::clicked,
		this, &PictureViewerUtilty::pbtn_exit_clicked);
	connect(ui->pbtn_previousimage, &QPushButton::clicked,
		this, &PictureViewerUtilty::pbtn_previousImage_clicked);
	connect(ui->pbtn_nextimage, &QPushButton::clicked,
		this, &PictureViewerUtilty::pbtn_nextImage_clicked);
	connect(ui->pbtn_delete, &QPushButton::clicked,
		this, &PictureViewerUtilty::pbtn_delete_clicked);
}

void PictureViewerUtilty::showEvent(QShowEvent* event)
{
	QFileInfo fileInfo(path);
	QString parentDir = fileInfo.dir().absolutePath();

	// 获取图片文件夹所有的图片路径
	setAllImgPath(parentDir, isPositive);

	// 获得当前图片的索引
	getCurrentImageIndex();

	QMainWindow::showEvent(event);
	if (!path.isEmpty() && ui->label_imgDisplay) {
		QPixmap pixmap(path);
		if (!pixmap.isNull()) {
			ui->label_imgDisplay->setPixmap(pixmap.scaled(ui->label_imgDisplay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		else {
			ui->label_imgDisplay->clear();
			ui->label_imgDisplay->setText("无法加载图片");
		}
	}
}

void PictureViewerUtilty::setImgPath(const QString& imgPath)
{
	path = imgPath;
}

void PictureViewerUtilty::setAllImgPath(const QString& imgParentPath, bool isPositive)
{
	// 定义图片扩展名过滤器
	QStringList filters;
	filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp" << "*.gif";

	QDir dir(imgParentPath);
	QStringList fileList;
	if (isPositive == true)
	{
		// 获取所有图片文件名，正序排列
		fileList = dir.entryList(filters, QDir::Files, QDir::Name);
	}
	else
	{
		// 获取所有图片文件名，倒序排列
		fileList = dir.entryList(filters, QDir::Files, QDir::Name);
		std::reverse(fileList.begin(), fileList.end());
	}

	for (const QString& fileName : fileList) {
		imagePaths.append(dir.absoluteFilePath(fileName));
	}
}

void PictureViewerUtilty::setPositive(bool ispositive = true)
{
	isPositive = ispositive;
}

int PictureViewerUtilty::getCurrentImageIndex()
{
	currentImageIndex = imagePaths.indexOf(path);
	return currentImageIndex;
}

void PictureViewerUtilty::pbtn_exit_clicked()
{
	emit imagesDeleted(deletedImagePaths);
	this->close();
}

void PictureViewerUtilty::pbtn_previousImage_clicked()
{
	if (imagePaths.isEmpty() || currentImageIndex <= 0)
	{
		QMessageBox::information(this, "提示", "已经是第一张图片了");
		return;
	}

	--currentImageIndex;
	path = imagePaths[currentImageIndex];

	if (ui->label_imgDisplay) {
		QPixmap pixmap(path);
		if (!pixmap.isNull()) {
			ui->label_imgDisplay->setPixmap(pixmap.scaled(ui->label_imgDisplay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		else {
			ui->label_imgDisplay->clear();
			ui->label_imgDisplay->setText("无法加载图片");
		}
	}
}

void PictureViewerUtilty::pbtn_nextImage_clicked()
{
	if (imagePaths.isEmpty())
		return;

	if (currentImageIndex >= imagePaths.size() - 1)
	{
		QMessageBox::information(this, "提示", "已经是最后一张图片了");
		return;
	}
	else
	{
		++currentImageIndex;
	}
	path = imagePaths[currentImageIndex];

	if (ui->label_imgDisplay) {
		QPixmap pixmap(path);
		if (!pixmap.isNull()) {
			ui->label_imgDisplay->setPixmap(pixmap.scaled(ui->label_imgDisplay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		else {
			ui->label_imgDisplay->clear();
			ui->label_imgDisplay->setText("无法加载图片");
		}
	}
}

void PictureViewerUtilty::pbtn_delete_clicked()
{
	if (imagePaths.isEmpty() || currentImageIndex < 0 || currentImageIndex >= imagePaths.size())
		return;

	QString fileToDelete = imagePaths[currentImageIndex];

	// 删除文件
	if (QFile::remove(fileToDelete)) {
		// 记录删除的图片路径
		deletedImagePaths.append(fileToDelete);

		imagePaths.removeAt(currentImageIndex);

		if (imagePaths.isEmpty()) {
			path.clear();
			if (ui->label_imgDisplay) {
				ui->label_imgDisplay->clear();
				ui->label_imgDisplay->setText("没有图片");
			}
			currentImageIndex = -1;
			return;
		}

		if (currentImageIndex >= imagePaths.size())
			currentImageIndex = imagePaths.size() - 1;

		path = imagePaths[currentImageIndex];

		if (ui->label_imgDisplay) {
			QPixmap pixmap(path);
			if (!pixmap.isNull()) {
				ui->label_imgDisplay->setPixmap(pixmap.scaled(ui->label_imgDisplay->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
			}
			else {
				ui->label_imgDisplay->clear();
				ui->label_imgDisplay->setText("无法加载图片");
			}
		}
	}
}