#pragma once

#include <QDialog>
#include "ui_Dlg_OffSet.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Dlg_OffSetClass; };
QT_END_NAMESPACE

class Dlg_OffSet : public QDialog
{
	Q_OBJECT

public:
	Dlg_OffSet(QWidget *parent = nullptr);
	~Dlg_OffSet();
private:
	void build_ui();
	void build_connect();

	void build_tempProcessParams();
protected:
	void showEvent(QShowEvent* event) override;
private slots:
	void btn_exit_clicked();

	void btn_zuoA_clicked();
	void btn_youA_clicked();
	void btn_zuoyouValueA_clicked();
	void btn_houA_clicked();
	void btn_qianA_clicked();
	void btn_qianhouValueA_clicked();
	void btn_zuozhuanA_clicked();
	void btn_youzhuanA_clicked();
	void btn_angleValueA_clicked();
	void btn_findNumberA_clicked();
	void btn_RotateAngleA_clicked();
	void ckb_usingA_checked(bool ischecked);

	void btn_zuoB_clicked();
	void btn_youB_clicked();
	void btn_zuoyouValueB_clicked();
	void btn_houB_clicked();
	void btn_qianB_clicked();
	void btn_qianhouValueB_clicked();
	void btn_zuozhuanB_clicked();
	void btn_youzhuanB_clicked();
	void btn_angleValueB_clicked();
	void btn_findNumberB_clicked();
	void btn_RotateAngleB_clicked();
	void ckb_usingB_checked(bool ischecked);

private:
	Ui::Dlg_OffSetClass *ui;
};

