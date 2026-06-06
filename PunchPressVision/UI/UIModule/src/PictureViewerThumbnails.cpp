#if 0
#include "PictureViewerThumbnails.h"

#include "ui_PictureViewerThumbnails.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QFileInfo>
#include <QImageReader>
#include <QPixmap>
#include <QIcon>
#include <QPainter>
#include <QDir>
#include <QMessageBox>
#include <QtConcurrent/qtconcurrentrun.h>

#include "dsl/Cache/dsl_CacheFIFO.hpp"

ThumbnailLoaderThread::ThumbnailLoaderThread(QQueue<QListWidgetItem*>* queue, QMutex* queueMutex,
	rw::dsl::CacheFIFO<QString, QPixmap>* cache, const QSize& thumbSize, QObject* uiReceiver)
	: m_queue(queue)
	, m_queueMutex(queueMutex)
	, m_cache(cache)
	, m_thumbSize(thumbSize)
	, m_uiReceiver(uiReceiver)
{
}

ThumbnailLoaderThread::~ThumbnailLoaderThread()
{
	stop();
	wait();
}

void ThumbnailLoaderThread::stop()
{
	m_running = false;
}

void ThumbnailLoaderThread::run()
{
	//QThread::sleep(1);
	while (m_running) {
		QListWidgetItem* item = nullptr;
		{
			QMutexLocker locker(m_queueMutex);
			if (m_queue->isEmpty())
				break;
			item = m_queue->dequeue();
		}
		if (!item) continue;
		QString path = item->data(Qt::UserRole).toString();

		QImageReader reader(path);

		reader.setAutoTransform(true);

		QImage img = reader.read();

		QPixmap squarePixmap;

		if (!img.isNull()) {
			QImage scaledImg = img.scaled(m_thumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			squarePixmap = QPixmap(m_thumbSize);
			squarePixmap.fill(Qt::transparent);
			QPainter painter(&squarePixmap);
			int x = (m_thumbSize.width() - scaledImg.width()) / 2;
			int y = (m_thumbSize.height() - scaledImg.height()) / 2;
			painter.drawImage(x, y, scaledImg);
			painter.end();
		}

		QIcon icon;
		if (!squarePixmap.isNull()) {
			m_cache->set(path, squarePixmap);
			icon = QIcon(squarePixmap);
		}

		emit iconReady(item, icon);
	}
}

void PictureViewerThumbnails::startAsyncLoadQueue()
{
	m_loaderThread = new ThumbnailLoaderThread(
		&disCacheImageItem,
		&disCacheImageItemMutex,
		_thumbnailCache.get(),
		big,
		this
	);
	connect(m_loaderThread, &ThumbnailLoaderThread::iconReady
		, this, &PictureViewerThumbnails::iconReady, Qt::QueuedConnection);
	m_loaderThread->start();
}

void PictureViewerThumbnails::stopAsyncLoadQueue()
{
	if (m_loaderThread) {
		m_loaderThread->stop();
		m_loaderThread->wait();
		disconnect(m_loaderThread, &ThumbnailLoaderThread::iconReady, this, &PictureViewerThumbnails::iconReady);
		delete m_loaderThread;
		m_loaderThread = nullptr;
	}
}

void PictureViewerThumbnails::setPositive(bool ispositive)
{
	isPositive = ispositive;
}

PictureViewerThumbnails::PictureViewerThumbnails(QWidget* parent)
	: QMainWindow(parent)
	, ui(new Ui::PictureViewerThumbnailsClass())
{
	ui->setupUi(this);

	build_ui();
	build_connect();
	isFirstLoad = true;
}

PictureViewerThumbnails::~PictureViewerThumbnails()
{
	stopAsyncLoadQueue();
	delete ui;
}

void PictureViewerThumbnails::setThumbnailCacheCapacity(size_t capacity)
{
	_thumbnailCacheCapacity = capacity;
	_thumbnailCache->resizeCapacity(_thumbnailCacheCapacity);
}

void PictureViewerThumbnails::setRootPath(const QString& rootPath)
{
	m_rootPath = rootPath;
}

void PictureViewerThumbnails::setSize(const QSize& size)
{
	m_thumbnailSize = size;
	_listWidget->setIconSize(m_thumbnailSize);

	QSize cellSize(m_thumbnailSize.width() + 16, m_thumbnailSize.height() + 32);
	for (int i = 0; i < _listWidget->count(); ++i) {
		QListWidgetItem* item = _listWidget->item(i);
		item->setSizeHint(cellSize);
		loadThumbnail(item->data(Qt::UserRole).toString(), item);
	}
}

void PictureViewerThumbnails::setSizeRange(const QSize& sizeSmall, const QSize& sizeBig)
{
	small = sizeSmall;
	big = sizeBig;
}

void PictureViewerThumbnails::setViewerNum(size_t num)
{
	m_viewerNum = num;
}

void PictureViewerThumbnails::showEvent(QShowEvent* event)
{
	_loadingDialog->updateMessage("加载图片中");
	_loadingDialog->show();
	updateCategoryList();

	QDir rootDir(m_rootPath);

	QtConcurrent::run([this, rootDir]() {
		preloadAllCategoryImages(rootDir);
		QMetaObject::invokeMethod(this, [this]() {
			loadImageList();
			_loadingDialog->close();
			}, Qt::QueuedConnection);
		});

	QMainWindow::showEvent(event);
}

void PictureViewerThumbnails::build_ui()
{
	_listWidget = new DraggableListWidget(this);
	auto listWidgetLayout = new QVBoxLayout(this);
	listWidgetLayout->addWidget(_listWidget);
	ui->groupBox->setLayout(listWidgetLayout);

	_listWidget->setViewMode(QListView::IconMode);
	_listWidget->setResizeMode(QListView::Adjust);
	_listWidget->setIconSize(m_thumbnailSize);
	_listWidget->setMovement(QListView::Static);
	_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	_listWidget->setSpacing(8);

	_loadingDialog = new LoadingDialog(this);
	_categoryModel = new QStandardItemModel(this);
	pictureViewerUtilty = new PictureViewerUtilty(this);
	pictureViewerUtilty->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint);
	ui->treeView_categoryTree->setModel(_categoryModel);

	_thumbnailCache = std::make_unique<rw::dsl::CacheFIFO<QString, QPixmap>>(_thumbnailCacheCapacity);
}

void PictureViewerThumbnails::build_connect()
{
	connect(_listWidget, &QListWidget::itemSelectionChanged,
		this, &PictureViewerThumbnails::onThumbnailSelected);

	connect(ui->pbtn_delete, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_delete_clicked);
	connect(ui->pbtn_deleteTotal, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_deleteTotal_clicked);
	connect(ui->pbtn_exit, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_exit_clicked);
	connect(ui->pbtn_nextPicture, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_nextPicture_clicked);
	connect(ui->pbtn_prePicture, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_prePicture_clicked);
	connect(ui->pbtn_bigger, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_bigger_clicked);
	connect(ui->pbtn_smaller, &QPushButton::clicked,
		this, &PictureViewerThumbnails::pbtn_smaller_clicked);

	connect(ui->treeView_categoryTree->selectionModel(), &QItemSelectionModel::currentChanged,
		this, &PictureViewerThumbnails::treeView_categoryTree_changed);

	connect(_listWidget, &QListWidget::itemDoubleClicked,
		this, &PictureViewerThumbnails::onThumbnailDoubleClicked);

	connect(pictureViewerUtilty, &PictureViewerUtilty::imagesDeleted,
		this, &PictureViewerThumbnails::updateImagesPaths);
}

void PictureViewerThumbnails::loadImageList()
{
	stopAsyncLoadQueue();
	{
		QMutexLocker locker(&disCacheImageItemMutex);
		//保证SetIcon槽函数全部执行完毕
		QCoreApplication::processEvents();
		disCacheImageItem.clear();
		_listWidget->clear();
		m_imageFiles.clear();
	}

	// 获取当前选中目录的路径
	QModelIndex currentCategoryIndex = ui->treeView_categoryTree->currentIndex();
	QString dirPath = m_rootPath;
	if (currentCategoryIndex.isValid()) {
		QVariant data = currentCategoryIndex.data(Qt::UserRole);
		if (data.isValid()) {
			dirPath = data.toString();
		}
	}

	// 只加载当前目录下的图片
	QStringList imageList = m_categoryImageCache.value(dirPath);

	size_t count = 0;
	for (const QString& imagePath : imageList) {
		if (m_viewerNum != 0 && count >= m_viewerNum)
		{
			break;
		}
		m_imageFiles << imagePath;
		QFileInfo fileInfo(imagePath);
		QListWidgetItem* item = new QListWidgetItem();
		item->setText(fileInfo.fileName());
		item->setData(Qt::UserRole, imagePath);
		item->setSizeHint(QSize(m_thumbnailSize.width() + 16, m_thumbnailSize.height() + 32));
		auto icon = _thumbnailCache->get(imagePath);
		if (icon.has_value()) {
			item->setIcon(QIcon(icon.value()));
		}
		else {
			item->setIcon(QIcon());
			{
				QMutexLocker locker(&disCacheImageItemMutex);
				disCacheImageItem.append(item);
				++count;
			}
		}
		_listWidget->addItem(item);
	}

	startAsyncLoadQueue();
}

bool PictureViewerThumbnails::loadThumbnail(const QString& imagePath, QListWidgetItem* item)
{
	auto icon = _thumbnailCache->get(imagePath);
	if (icon.has_value())
	{
		item->setIcon(QIcon(icon.value()));
		return true;
	}
	else
	{
		disCacheImageItem.append(item);
		return false;
	}
}

void PictureViewerThumbnails::updateCategoryList()
{
	_categoryModel->clear();
	// 检查 _rootPath 是否有效
	if (m_rootPath.isEmpty()) {
		qDebug() << "Root path is empty.";
		return;
	}

	QDir rootDir(m_rootPath);
	if (!rootDir.exists()) {
		qDebug() << "Root path does not exist:" << m_rootPath;
		return;
	}

	_categoryModel->setHorizontalHeaderLabels({ "文件树" });

	// 遍历 _rootPath 下的所有文件夹
	QStringList categoryFolders = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	if (categoryFolders.isEmpty()) {
		// 没有子目录，直接添加rootDir本身
		QStandardItem* rootItem = new QStandardItem(rootDir.dirName());
		rootItem->setData(rootDir.absolutePath(), Qt::UserRole);
		_categoryModel->appendRow(rootItem);
	}
	else {
		for (const QString& category : categoryFolders) {
			QString categoryPath = rootDir.filePath(category);
			QStandardItem* categoryItem = new QStandardItem(category);

			// 保存绝对路径到节点
			categoryItem->setData(categoryPath, Qt::UserRole);

			// 递归添加子文件夹
			QDir categoryDir(categoryPath);
			addSubFolders(categoryDir, categoryItem);

			_categoryModel->appendRow(categoryItem);
		}
	}

	// 设置模型到 treeView_categoryTree

	ui->treeView_categoryTree->expandAll();

	// 设置当前选中的索引为第一个最深的子节点
	QModelIndex firstDeepestIndex = findFirstDeepestIndex(_categoryModel);
	if (firstDeepestIndex.isValid()) {
		ui->treeView_categoryTree->setCurrentIndex(firstDeepestIndex);
	}
}

void PictureViewerThumbnails::addSubFolders(const QDir& parentDir, QStandardItem* parentItem)
{
	// 获取当前目录下的所有子文件夹
	QStringList subFolders = parentDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QString& subFolder : subFolders) {
		QString subFolderPath = parentDir.filePath(subFolder);
		QStandardItem* subFolderItem = new QStandardItem(subFolder);

		// 保存绝对路径到节点
		subFolderItem->setData(subFolderPath, Qt::UserRole);

		// 递归处理子文件夹
		QDir subFolderDir(subFolderPath);
		addSubFolders(subFolderDir, subFolderItem);

		parentItem->appendRow(subFolderItem);
	}
}

QModelIndex PictureViewerThumbnails::findFirstDeepestIndex(QStandardItemModel* model)
{
	if (!model || model->rowCount() == 0) {
		return QModelIndex();
	}

	QStandardItem* rootItem = model->invisibleRootItem();
	return findDeepestChild(rootItem);
}

QModelIndex PictureViewerThumbnails::findDeepestChild(QStandardItem* parentItem)
{
	if (!parentItem || parentItem->rowCount() == 0) {
		return parentItem ? parentItem->index() : QModelIndex();
	}

	// 遍历子节点，找到第一个最深的子节点
	QStandardItem* firstChild = parentItem->child(0);
	return findDeepestChild(firstChild);
}

void PictureViewerThumbnails::preloadAllCategoryImages(const QDir& dir)
{
	QStringList nameFilters;
	const auto formats = QImageReader::supportedImageFormats();
	for (const QByteArray& fmt : formats)
		nameFilters << "*." + fmt;

	QFileInfoList fileList = dir.entryInfoList(nameFilters, QDir::Files | QDir::NoSymLinks | QDir::Readable, QDir::Name);
	QStringList imagePaths;
	for (const QFileInfo& fileInfo : fileList) {
		imagePaths << fileInfo.absoluteFilePath();
	}
	m_categoryImageCache.insert(dir.absolutePath(), imagePaths);

	// 递归处理子目录
	QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QString& subDir : subDirs) {
		QDir subDirPath(dir.filePath(subDir));
		preloadAllCategoryImages(subDirPath);
	}
}

void PictureViewerThumbnails::onThumbnailSelected()
{
	QListWidgetItem* item = _listWidget->currentItem();
	if (item) {
		QString path = item->data(Qt::UserRole).toString();
		ui->statusBar->showMessage(path);
	}
}

void PictureViewerThumbnails::pbtn_exit_clicked()
{
	stopAsyncLoadQueue();
	this->close();
	emit viewerClosed();
}

void PictureViewerThumbnails::pbtn_deleteTotal_clicked()
{
	stopAsyncLoadQueue();
	// 询问用户是否删除当前目录下所有图片
	auto result = QMessageBox::question(
		this, "提示", "是否删除当前目录下所有图片？",
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if (result == QMessageBox::No) {
		return;
	}

	// 获取当前选中目录的绝对路径
	QModelIndex currentCategoryIndex = ui->treeView_categoryTree->currentIndex();
	QString categoryPath = m_rootPath;
	if (currentCategoryIndex.isValid()) {
		QVariant data = currentCategoryIndex.data(Qt::UserRole);
		if (data.isValid()) {
			categoryPath = data.toString();
		}
	}
	QDir dir(categoryPath);

	// 检查目录是否存在
	if (!dir.exists()) {
		qDebug() << "Directory does not exist:" << categoryPath;
		return;
	}

	// 删除目录中的所有文件
	QStringList fileFilters;
	fileFilters << "*"; // 匹配所有文件
	QStringList files = dir.entryList(fileFilters, QDir::Files | QDir::NoSymLinks);
	for (const QString& file : files) {
		QString filePath = dir.filePath(file);
		if (!QFile::remove(filePath)) {
			qDebug() << "Failed to delete file:" << filePath;
		}
		else {
			qDebug() << "Deleted file:" << filePath;
			// m_thumbnailCache->remove(filePath); // 如需移除缩略图缓存可放开
		}
	}

	// 删除目录中的所有子文件夹及其内容
	QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QString& subDir : subDirs) {
		QDir subDirPath(dir.filePath(subDir));
		if (!subDirPath.removeRecursively()) {
			qDebug() << "Failed to delete subdirectory:" << subDirPath.path();
		}
		else {
			qDebug() << "Deleted subdirectory:" << subDirPath.path();
		}
	}

	// 清除该目录及其子目录的图片路径缓存
	auto it = m_categoryImageCache.begin();
	while (it != m_categoryImageCache.end()) {
		const QString& key = it.key();
		if (key == categoryPath || key.startsWith(categoryPath + "/")) {
			it = m_categoryImageCache.erase(it);
		}
		else {
			++it;
		}
	}

	// 清理 disCacheImageItem 中属于该目录的 item
	{
		QMutexLocker locker(&disCacheImageItemMutex);
		for (int i = disCacheImageItem.size() - 1; i >= 0; --i) {
			QListWidgetItem* item = disCacheImageItem[i];
			if (item && item->data(Qt::UserRole).toString().startsWith(categoryPath)) {
				disCacheImageItem.removeAt(i);
			}
		}
	}

	// 刷新 UI
	loadImageList();
}

void PictureViewerThumbnails::pbtn_delete_clicked()
{
	stopAsyncLoadQueue();

	// 获取当前选中的缩略图项
	QListWidgetItem* item = _listWidget->currentItem();
	if (!item) {
		startAsyncLoadQueue();
		return;
	}

	QString imagePath = item->data(Qt::UserRole).toString();

	// 删除文件
	QFile::remove(imagePath);

	// 从 m_imageFiles 移除
	m_imageFiles.removeAll(imagePath);

	// 从 m_categoryImageCache 移除
	for (auto it = m_categoryImageCache.begin(); it != m_categoryImageCache.end(); ++it) {
		it.value().removeAll(imagePath);
	}

	// 从 disCacheImageItem 移除
	{
		QMutexLocker locker(&disCacheImageItemMutex);
		for (int i = disCacheImageItem.size() - 1; i >= 0; --i) {
			QListWidgetItem* cacheItem = disCacheImageItem[i];
			if (cacheItem && cacheItem->data(Qt::UserRole).toString() == imagePath) {
				disCacheImageItem.removeAt(i);
			}
		}
	}

	// 从 UI 移除
	delete _listWidget->takeItem(_listWidget->row(item));

	startAsyncLoadQueue();
}

void PictureViewerThumbnails::pbtn_prePicture_clicked()
{
	int count = _listWidget->count();
	if (count == 0) return;

	int currentRow = _listWidget->currentRow();
	if (currentRow == -1) {
		// 没有选中任何项，默认选中第一个
		_listWidget->setCurrentRow(0);
		return;
	}
	if (currentRow > 0) {
		_listWidget->setCurrentRow(currentRow - 1);
	}
	// 如果已经是第一个，则不做处理（也可循环到最后一个，根据需求调整）
}

void PictureViewerThumbnails::pbtn_nextPicture_clicked()
{
	int count = _listWidget->count();
	if (count == 0) return;

	int currentRow = _listWidget->currentRow();
	if (currentRow == -1) {
		// 没有选中任何项，默认选中第一个
		_listWidget->setCurrentRow(0);
		return;
	}
	if (currentRow < count - 1) {
		_listWidget->setCurrentRow(currentRow + 1);
	}
	// 如果已经是最后一个，则不做处理（也可循环到第一个，根据需求调整）
}

void PictureViewerThumbnails::pbtn_bigger_clicked()
{
	QSize nextSize = { m_thumbnailSize.width() + 30 ,m_thumbnailSize.height() + 30 };
	if (small.width() > nextSize.width())
	{
		if (small.height() > nextSize.height())
		{
			return;
		}
	}

	if (nextSize.width() > big.width())
	{
		if (nextSize.height() > big.height())
		{
			return;
		}
	}
	m_thumbnailSize = nextSize;
	setSize(m_thumbnailSize);
}

void PictureViewerThumbnails::pbtn_smaller_clicked()
{
	QSize nextSize = { m_thumbnailSize.width() - 30 ,m_thumbnailSize.height() - 30 };
	if (small.width() > nextSize.width())
	{
		if (small.height() > nextSize.height())
		{
			return;
		}
	}

	if (nextSize.width() > big.width())
	{
		if (nextSize.height() > big.height())
		{
			return;
		}
	}
	m_thumbnailSize = nextSize;
	setSize(m_thumbnailSize);
}

void PictureViewerThumbnails::updateImagesPaths(QVector<QString> imagesPaths)
{
	stopAsyncLoadQueue();

	for (const auto& pathItem : imagesPaths)
	{
		QString imagePath = pathItem;

		// 删除文件
		QFile::remove(imagePath);

		// 从 m_imageFiles 移除
		m_imageFiles.removeAll(imagePath);

		// 从 m_categoryImageCache 移除
		for (auto it = m_categoryImageCache.begin(); it != m_categoryImageCache.end(); ++it) {
			it.value().removeAll(imagePath);
		}

		// 从 disCacheImageItem 移除
		{
			QMutexLocker locker(&disCacheImageItemMutex);
			for (int i = disCacheImageItem.size() - 1; i >= 0; --i) {
				QListWidgetItem* cacheItem = disCacheImageItem[i];
				if (cacheItem && cacheItem->data(Qt::UserRole).toString() == imagePath) {
					disCacheImageItem.removeAt(i);
				}
			}
		}

		// 从 UI 移除
		for (int i = _listWidget->count() - 1; i >= 0; --i) {
			QListWidgetItem* item = _listWidget->item(i);
			if (item && item->data(Qt::UserRole).toString() == imagePath) {
				delete _listWidget->takeItem(i);
			}
		}
	}

	startAsyncLoadQueue();
}

void PictureViewerThumbnails::onThumbnailDoubleClicked(QListWidgetItem* item)
{
	if (!item) return;
	QString imagePath = item->data(Qt::UserRole).toString();
	pictureViewerUtilty->setPositive(isPositive);
	pictureViewerUtilty->setImgPath(imagePath);
	pictureViewerUtilty->show();
}

void PictureViewerThumbnails::treeView_categoryTree_changed(const QModelIndex& current, const QModelIndex&)
{
	Q_UNUSED(current);
	if (isFirstLoad)
	{
		isFirstLoad = false;
		return;
	}

	loadImageList();

	if (_listWidget->count() > 0) {
		_listWidget->setCurrentRow(0);
	}
}

void PictureViewerThumbnails::iconReady(QListWidgetItem* item, const QIcon& icon)
{
	item->setIcon(icon);
}

#endif