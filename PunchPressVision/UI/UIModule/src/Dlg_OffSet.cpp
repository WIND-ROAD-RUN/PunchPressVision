#include "Dlg_OffSet.h"
#include "ui_Dlg_OffSet.h"
#if 0 // --- 以下项目引用暂时注释 ---
#include <rqwu/rqwu_MessageBox.h>
#include <rqwu/Keyboard/rqwu_NumberKeyboard.h>
#include "Modules.hpp"
#endif

Dlg_OffSet::Dlg_OffSet(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_OffSetClass())
{
	ui->setupUi(this);
#if 0 // --- 以下内容暂时注释 ---
	build_ui();
	build_connect();
#endif
}

Dlg_OffSet::~Dlg_OffSet()
{
	delete ui;
}

#if 0 // --- 以下方法实现暂时注释 ---
// build_ui, build_connect, build_tempProcessParams,
// showEvent, btn_exit_clicked,
// btn_zuoA_clicked, btn_youA_clicked, btn_zuoyouValueA_clicked,
// btn_houA_clicked, btn_qianA_clicked, btn_qianhouValueA_clicked,
// btn_zuozhuanA_clicked, btn_youzhuanA_clicked, btn_angleValueA_clicked,
// btn_findNumberA_clicked, btn_RotateAngleA_clicked, ckb_usingA_checked,
// btn_zuoB_clicked, btn_youB_clicked, btn_zuoyouValueB_clicked,
// btn_houB_clicked, btn_qianB_clicked, btn_qianhouValueB_clicked,
// btn_zuozhuanB_clicked, btn_youzhuanB_clicked, btn_angleValueB_clicked,
// btn_findNumberB_clicked, btn_RotateAngleB_clicked, ckb_usingB_checked
#endif
