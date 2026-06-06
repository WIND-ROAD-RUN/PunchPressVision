#include "ImageEnlargedDisplay.h"
#include "ui_rqw_ImageEnlargedDisplay.h"

ImageEnlargedDisplay::ImageEnlargedDisplay(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui::rqw_ImageEnlargedDisplayClass())
{
	ui->setupUi(this);

	build_ui();
	build_connect();
}

ImageEnlargedDisplay::~ImageEnlargedDisplay()
{
	delete ui;
}

void ImageEnlargedDisplay::build_ui()
{
	this->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
}

void ImageEnlargedDisplay::build_connect()
{
	connect(ui->pbtn_exit, &QPushButton::clicked,
		this, &ImageEnlargedDisplay::pbtn_exit_clicked);
	connect(ui->pbtn_nextWork, &QPushButton::clicked,
		this, &ImageEnlargedDisplay::pbtn_nextWork_clicked);
	connect(ui->pbtn_preWork, &QPushButton::clicked,
		this, &ImageEnlargedDisplay::pbtn_preWork_clicked);
}

void ImageEnlargedDisplay::initWorkStationTitleMap(const std::map<int, QString>& map)
{
	_workStationTitleMap = map;
}

void ImageEnlargedDisplay::initWorkStationTitleMap(const QMap<int, QString>& map)
{
	_workStationTitleMap.clear();
	for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
		_workStationTitleMap[it.key()] = it.value();
	}
}

void ImageEnlargedDisplay::initWorkStationImageMap(const std::map<int, QPixmap>& map)
{
	_workStationImageMap = map;
}

void ImageEnlargedDisplay::initWorkStationImageMap(const QMap<int, QPixmap>& map)
{
	_workStationImageMap.clear();
	for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
		_workStationImageMap[it.key()] = it.value();
	}
}

void ImageEnlargedDisplay::setNum(int number)
{
	if (number > 0)
	{
		num = number;
	}
}

void ImageEnlargedDisplay::setMonitorValue(bool* isShow)
{
	if (!isShow)
	{
		return;
	}
	_isShow = isShow;
}

void ImageEnlargedDisplay::setMonitorDisImgIndex(int* index)
{
	if (!index)
	{
		return;
	}
	_currentDisImgIndex = index;
}

void ImageEnlargedDisplay::setGboxTitle(const QString& title)
{
	ui->gBox_imgDis->setTitle(title);
}

void ImageEnlargedDisplay::showEvent(QShowEvent* event)
{
	if (_isShow)
	{
		*_isShow = true;
	}
}

void ImageEnlargedDisplay::setShowImg(const QPixmap& image)
{
	ui->label_imgDis->setPixmap(image.scaled(ui->label_imgDis->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void ImageEnlargedDisplay::clearImgDis()
{
	ui->label_imgDis->clear();
	ui->label_imgDis->setPixmap(QPixmap());
}

void ImageEnlargedDisplay::updateTitle(int index)
{
	auto it = _workStationTitleMap.find(index);
	if (it != _workStationTitleMap.end()) {
		setGboxTitle(it->second);
	}
}

void ImageEnlargedDisplay::updateImage(int index)
{
	auto it = _workStationImageMap.find(index);
	if (it != _workStationImageMap.end()) {
		setShowImg(it->second);
	}
}

void ImageEnlargedDisplay::pbtn_exit_clicked()
{
	if (_isShow)
	{
		*_isShow = false;
	}
	this->close();
}

void ImageEnlargedDisplay::pbtn_nextWork_clicked()
{
	if (!_currentDisImgIndex)
	{
		return;
	}
	auto index = *_currentDisImgIndex;
	index += 1;
	index = (index + num) % num;
	updateTitle(index);
	updateImage(index);
	*_currentDisImgIndex = index;
}

void ImageEnlargedDisplay::pbtn_preWork_clicked()
{
	if (!_currentDisImgIndex)
	{
		return;
	}
	auto index = *_currentDisImgIndex;
	index -= 1;
	index = (index + num) % num;
	updateTitle(index);
	updateImage(index);
	*_currentDisImgIndex = index;
}

void ImageEnlargedDisplay::closeEvent(QCloseEvent* close_event)
{
	if (_isShow)
	{
		*_isShow = false;
	}
	QDialog::closeEvent(close_event);
}

