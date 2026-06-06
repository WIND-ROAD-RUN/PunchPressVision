#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include <deque>
#include <QDateTime>
#include"WarnUtilty.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class WarningInfoListClass; }
QT_END_NAMESPACE

class WarningInfoList : public QDialog
{
	Q_OBJECT

public:
	explicit WarningInfoList(QWidget* parent = nullptr);
	~WarningInfoList();
private:
	void build_ui();
	void build_connect();
public:

	// 设置警告信息
	void setWarningHistory(const std::deque<rw::rqw::WarningInfo>& history);

signals:
	// 信号：清空警告信息
	void clearWarnings();

private slots:
	// 槽函数：清空警告信息
	void onClearWarnings();

private:
	Ui::WarningInfoListClass* ui;
	QStandardItemModel* _model; // 数据模型
};