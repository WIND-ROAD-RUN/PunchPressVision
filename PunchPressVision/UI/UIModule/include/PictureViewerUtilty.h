#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class PictureViewerUtiltyClass; };
QT_END_NAMESPACE

class PictureViewerUtilty : public QMainWindow
{
	Q_OBJECT

public:
	PictureViewerUtilty(QWidget* parent = nullptr);
	~PictureViewerUtilty();
private:
	void build_ui();
	void build_connect();
protected:
	void showEvent(QShowEvent* event) override;
public:
	void setImgPath(const QString& imgPath);
	void setAllImgPath(const QString& imgPath, bool isPositive);
	void setPositive(bool ispositive);
	int getCurrentImageIndex();
private:
	QString path;
	QVector<QString> imagePaths;
	QVector<QString> deletedImagePaths;
	bool isPositive = true;
	int currentImageIndex = 0;
private:
	Ui::PictureViewerUtiltyClass* ui;

signals:
	void imagesDeleted(QVector<QString> ImagePaths);

private slots:
	void pbtn_exit_clicked();
	void pbtn_previousImage_clicked();
	void pbtn_nextImage_clicked();
	void pbtn_delete_clicked();
};
