#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class DlgCloseFormClass; };
QT_END_NAMESPACE

class DlgCloseForm : public QDialog
{
	Q_OBJECT

public:
	DlgCloseForm(QWidget *parent = nullptr);
	~DlgCloseForm();
private:
	void build_connect();
public:
	void setLabelShowMessage(QString message);
private slots:
	void btn_close_clicked();
	void btn_restart_clicked();
	void btn_yes_clicked();
	void btn_no_clicked();
private:
	Ui::DlgCloseFormClass *ui;
};

