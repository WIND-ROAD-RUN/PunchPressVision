#pragma once

#include <QDir>
#include <QMainWindow>
#include <QStandardItemModel>

#include"DraggableListWidget.h"
#include"LoadingDialog.h"
#include"PictureViewerUtilty.h"

#include<QListWidgetItem>
#include <QMutex>
#include <QQueue>
#include<QThread>

QT_BEGIN_NAMESPACE
namespace Ui { class PictureViewerThumbnailsClass; };
QT_END_NAMESPACE

namespace rw
{
	namespace dsl
	{
		template <typename Key, typename Value>
		class CacheFIFO;
	}
}

class ThumbnailLoaderThread : public QThread
{
	Q_OBJECT
public:
	ThumbnailLoaderThread(
		QQueue<QListWidgetItem*>* queue,
		QMutex* queueMutex,
		rw::dsl::CacheFIFO<QString, QPixmap>* cache,
		const QSize& thumbSize,
		QObject* uiReceiver // 用于invokeMethod
	);
	~ThumbnailLoaderThread() override;
	void stop();

protected:
	void run() override;
signals:
	void iconReady(QListWidgetItem* item, const QIcon& icon);

private:
	QQueue<QListWidgetItem*>* m_queue;
	QMutex* m_queueMutex;
	rw::dsl::CacheFIFO<QString, QPixmap>* m_cache;
	QSize m_thumbSize;
	QObject* m_uiReceiver;
	std::atomic<bool> m_running{ true };
};

class PictureViewerThumbnails : public QMainWindow
{
	Q_OBJECT
private:
	bool isFirstLoad{ false };
public:
	void startAsyncLoadQueue();
	void stopAsyncLoadQueue();
private:
	QQueue<QListWidgetItem*> disCacheImageItem;
	QMutex disCacheImageItemMutex;
	QSet<QString> loadingSet;
public:
	void setPositive(bool ispositive);
private:
	bool isPositive = true;
private:
	ThumbnailLoaderThread* m_loaderThread = nullptr;
private:
	std::unique_ptr<rw::dsl::CacheFIFO<QString, QPixmap>> _thumbnailCache;
private:
	QHash<QString, QStringList> m_categoryImageCache;
private:
	LoadingDialog* _loadingDialog = nullptr;
	PictureViewerUtilty* pictureViewerUtilty = nullptr;
public:
	PictureViewerThumbnails(QWidget* parent = nullptr);
	~PictureViewerThumbnails() override;
public:
	void setThumbnailCacheCapacity(size_t capacity);
private:
	size_t _thumbnailCacheCapacity{ 1000 };
public:
	void setRootPath(const QString& rootPath);

	void setSize(const QSize& size);
	void setSizeRange(const QSize& sizeSmall, const QSize& sizeBig);
	void setViewerNum(size_t num = 0);
private:
	QSize small{ 100,100 };
	QSize big{ 500,500 };
signals:
	void viewerClosed();
protected:
	void showEvent(QShowEvent* event) override;
private:
	void build_ui();
	void build_connect();
private:
	void loadImageList();
	bool loadThumbnail(const QString& imagePath, QListWidgetItem* item);
	void updateCategoryList();
	void addSubFolders(const QDir& parentDir, QStandardItem* parentItem);
	QModelIndex findFirstDeepestIndex(QStandardItemModel* model);
	QModelIndex findDeepestChild(QStandardItem* parentItem);
private:
	void preloadAllCategoryImages(const QDir& dir);

private:
	Ui::PictureViewerThumbnailsClass* ui;
private:

	QString m_rootPath;
	QSize m_thumbnailSize{ 128, 128 };
	QStringList m_imageFiles;
	size_t m_viewerNum{ 0 };
private:
	QStandardItemModel* _categoryModel;
	DraggableListWidget* _listWidget = nullptr;
private slots:
	void onThumbnailSelected();
	void pbtn_exit_clicked();
	void pbtn_deleteTotal_clicked();
	void pbtn_delete_clicked();
	void pbtn_prePicture_clicked();
	void pbtn_nextPicture_clicked();
	void pbtn_bigger_clicked();
	void pbtn_smaller_clicked();

	void updateImagesPaths(QVector<QString> imagesPaths);
private slots:
	void onThumbnailDoubleClicked(QListWidgetItem* item);
private slots:
	void treeView_categoryTree_changed(const QModelIndex& current, const QModelIndex&);

public slots:
	void iconReady(QListWidgetItem* item, const QIcon& icon);
};
