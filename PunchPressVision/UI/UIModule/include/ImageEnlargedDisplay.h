#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class rqw_ImageEnlargedDisplayClass; };
QT_END_NAMESPACE

class ImageEnlargedDisplay : public QDialog
{
	Q_OBJECT

public:
	ImageEnlargedDisplay(QWidget* parent = nullptr);
	~ImageEnlargedDisplay();
private:
	void build_ui();
	void build_connect();
private:
	bool* _isShow;
	int* _currentDisImgIndex;
	int num{ 1 };
	std::map<int, QString> _workStationTitleMap{};
	std::map<int, QPixmap> _workStationImageMap{};
public:
	void initWorkStationTitleMap(const std::map<int, QString>& map);
	void initWorkStationTitleMap(const QMap<int, QString>& map);
	void initWorkStationImageMap(const std::map<int, QPixmap>& map);
	void initWorkStationImageMap(const QMap<int, QPixmap>& map);
public:
	void setNum(int number);
public:
	void setMonitorValue(bool* isShow);
	void setMonitorDisImgIndex(int* index);
public:
	void setGboxTitle(const QString& title);
protected:
	void showEvent(QShowEvent* event) override;
public:
	void setShowImg(const QPixmap& image);
	void clearImgDis();
public:
	void updateTitle(int index);
	void updateImage(int index);
public slots:
	void pbtn_exit_clicked();
	void pbtn_nextWork_clicked();
	void pbtn_preWork_clicked();

protected:
	void closeEvent(QCloseEvent*) override;

private:
	Ui::rqw_ImageEnlargedDisplayClass* ui;
};

