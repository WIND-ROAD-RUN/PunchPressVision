#include "Dlg_OffSet.h"

#include <rqwu/rqwu_MessageBox.h>
#include <rqwu/Keyboard/rqwu_NumberKeyboard.h>
#include "Modules.hpp"

Dlg_OffSet::Dlg_OffSet(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::Dlg_OffSetClass())
{
	ui->setupUi(this);

	build_ui();

	build_connect();
}

Dlg_OffSet::~Dlg_OffSet()
{
	delete ui;
}

void Dlg_OffSet::build_ui()
{
	build_tempProcessParams();
}

void Dlg_OffSet::build_connect()
{
	connect(ui->btn_exit, &QPushButton::clicked, this, &Dlg_OffSet::btn_exit_clicked);

	connect(ui->btn_zuoA, &QPushButton::clicked, this, &Dlg_OffSet::btn_zuoA_clicked);
	connect(ui->btn_youA, &QPushButton::clicked, this, &Dlg_OffSet::btn_youA_clicked);
	connect(ui->btn_zuoyouValueA, &QPushButton::clicked, this, &Dlg_OffSet::btn_zuoyouValueA_clicked);
	connect(ui->btn_houA, &QPushButton::clicked, this, &Dlg_OffSet::btn_houA_clicked);
	connect(ui->btn_qianA, &QPushButton::clicked, this, &Dlg_OffSet::btn_qianA_clicked);
	connect(ui->btn_qianhouValueA, &QPushButton::clicked, this, &Dlg_OffSet::btn_qianhouValueA_clicked);
	connect(ui->btn_zuozhuanA, &QPushButton::clicked, this, &Dlg_OffSet::btn_zuozhuanA_clicked);
	connect(ui->btn_youzhuanA, &QPushButton::clicked, this, &Dlg_OffSet::btn_youzhuanA_clicked);
	connect(ui->btn_angleValueA, &QPushButton::clicked, this, &Dlg_OffSet::btn_angleValueA_clicked);
	connect(ui->btn_findNumberA, &QPushButton::clicked, this, &Dlg_OffSet::btn_findNumberA_clicked);
	connect(ui->btn_RotateAngleA, &QPushButton::clicked, this, &Dlg_OffSet::btn_RotateAngleA_clicked);
	connect(ui->ckb_usingA, &QPushButton::clicked, this, &Dlg_OffSet::ckb_usingA_checked);

	connect(ui->btn_zuoB, &QPushButton::clicked, this, &Dlg_OffSet::btn_zuoB_clicked);
	connect(ui->btn_youB, &QPushButton::clicked, this, &Dlg_OffSet::btn_youB_clicked);
	connect(ui->btn_zuoyouValueB, &QPushButton::clicked, this, &Dlg_OffSet::btn_zuoyouValueB_clicked);
	connect(ui->btn_houB, &QPushButton::clicked, this, &Dlg_OffSet::btn_houB_clicked);
	connect(ui->btn_qianB, &QPushButton::clicked, this, &Dlg_OffSet::btn_qianB_clicked);
	connect(ui->btn_qianhouValueB, &QPushButton::clicked, this, &Dlg_OffSet::btn_qianhouValueB_clicked);
	connect(ui->btn_zuozhuanB, &QPushButton::clicked, this, &Dlg_OffSet::btn_zuozhuanB_clicked);
	connect(ui->btn_youzhuanB, &QPushButton::clicked, this, &Dlg_OffSet::btn_youzhuanB_clicked);
	connect(ui->btn_angleValueB, &QPushButton::clicked, this, &Dlg_OffSet::btn_angleValueB_clicked);
	connect(ui->btn_findNumberB, &QPushButton::clicked, this, &Dlg_OffSet::btn_findNumberB_clicked);
	connect(ui->btn_RotateAngleB, &QPushButton::clicked, this, &Dlg_OffSet::btn_RotateAngleB_clicked);
	connect(ui->ckb_usingB, &QPushButton::clicked, this, &Dlg_OffSet::ckb_usingB_checked);
}

void Dlg_OffSet::build_tempProcessParams()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	ProcessParam tempProcessParam;
	tempProcessParam.enabled = false;
	processParam.append(tempProcessParam);
	processParam.append(tempProcessParam);
}

void Dlg_OffSet::showEvent(QShowEvent* event)
{
	QDialog::showEvent(event);

	ui->tabWidget_2->setCurrentIndex(0);

	auto& processParam = Modules::getInstance().configManagerModule.processParams;

	ui->btn_zuoyouValueA->setText(QString::number(processParam[0].offsetX));
	ui->btn_qianhouValueA->setText(QString::number(processParam[0].offsetY));
	ui->btn_angleValueA->setText(QString::number(processParam[0].offsetAngle));
	ui->btn_findNumberA->setText(QString::number(processParam[0].findnumber));
	ui->ckb_usingA->setChecked(processParam[0].enabled);
	ui->btn_RotateAngleA->setText(QString::number(processParam[0].rotateAngle));

	ui->btn_zuoyouValueB->setText(QString::number(processParam[1].offsetX));
	ui->btn_qianhouValueB->setText(QString::number(processParam[1].offsetY));
	ui->btn_angleValueB->setText(QString::number(processParam[1].offsetAngle));
	ui->btn_findNumberB->setText(QString::number(processParam[1].findnumber));
	ui->ckb_usingB->setChecked(processParam[1].enabled);
	ui->btn_RotateAngleB->setText(QString::number(processParam[1].rotateAngle));
}

void Dlg_OffSet::btn_exit_clicked()
{
	this->close();
}

void Dlg_OffSet::btn_zuoA_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].offsetX -= 0.1f;
	ui->btn_zuoyouValueA->setText(QString::number(processParam[0].offsetX));
}

void Dlg_OffSet::btn_youA_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].offsetX += 0.1f;
	ui->btn_zuoyouValueA->setText(QString::number(processParam[0].offsetX));
}

void Dlg_OffSet::btn_zuoyouValueA_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[0].offsetX = value.toDouble();
		ui->btn_zuoyouValueA->setText(value);
	}
}

void Dlg_OffSet::btn_houA_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].offsetY -= 0.1f;
	ui->btn_qianhouValueA->setText(QString::number(processParam[0].offsetY));
}

void Dlg_OffSet::btn_qianA_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].offsetY += 0.1f;
	ui->btn_qianhouValueA->setText(QString::number(processParam[0].offsetY));
}

void Dlg_OffSet::btn_qianhouValueA_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[0].offsetY = value.toDouble();
		ui->btn_qianhouValueA->setText(value);
	}
}

void Dlg_OffSet::btn_zuozhuanA_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].offsetAngle -= 0.1;
	ui->btn_angleValueA->setText(QString::number(processParam[0].offsetAngle));
}

void Dlg_OffSet::btn_youzhuanA_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].offsetAngle += 0.1;
	ui->btn_angleValueA->setText(QString::number(processParam[0].offsetAngle));
}

void Dlg_OffSet::btn_angleValueA_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[0].offsetAngle = value.toDouble();
		ui->btn_angleValueA->setText(value);
	}
}

void Dlg_OffSet::btn_zuoB_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].offsetX -= 0.1f;
	ui->btn_zuoyouValueB->setText(QString::number(processParam[1].offsetX));
}

void Dlg_OffSet::btn_youB_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].offsetX += 0.1f;
	ui->btn_zuoyouValueB->setText(QString::number(processParam[1].offsetX));
}

void Dlg_OffSet::btn_zuoyouValueB_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[1].offsetX = value.toDouble();
		ui->btn_zuoyouValueB->setText(value);
	}
}

void Dlg_OffSet::btn_houB_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].offsetY -= 0.1f;
	ui->btn_qianhouValueB->setText(QString::number(processParam[1].offsetY));
}

void Dlg_OffSet::btn_qianB_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].offsetY += 0.1f;
	ui->btn_qianhouValueB->setText(QString::number(processParam[1].offsetY));
}

void Dlg_OffSet::btn_qianhouValueB_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[1].offsetY = value.toDouble();
		ui->btn_qianhouValueB->setText(value);
	}
}

void Dlg_OffSet::btn_zuozhuanB_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].offsetAngle -= 0.1;
	ui->btn_angleValueB->setText(QString::number(processParam[1].offsetAngle));
}

void Dlg_OffSet::btn_youzhuanB_clicked()
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].offsetAngle += 0.1;
	ui->btn_angleValueB->setText(QString::number(processParam[1].offsetAngle));
}

void Dlg_OffSet::btn_angleValueB_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[1].offsetAngle = value.toDouble();
		ui->btn_angleValueB->setText(value);
	}
}

void Dlg_OffSet::btn_findNumberA_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[0].findnumber = value.toDouble();
		ui->btn_findNumberA->setText(value);
	}
}

void Dlg_OffSet::btn_RotateAngleA_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 0.0 || value.toDouble() > 360.0)
		{
			rw::rqwu::MessageBox::warning(this, "警告", "请输入0~180范围的角度!");
		}
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[0].rotateAngle = value.toDouble();
		ui->btn_RotateAngleA->setText(value);
	}
}

void Dlg_OffSet::btn_findNumberB_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[1].findnumber = value.toDouble();
		ui->btn_findNumberB->setText(value);
	}
}

void Dlg_OffSet::btn_RotateAngleB_clicked()
{
	rw::rqwu::NumberKeyboard numKeyBord;
	auto isAccept = numKeyBord.exec();
	if (isAccept == QDialog::Accepted)
	{
		auto value = numKeyBord.getValue();
		if (value.toDouble() < 0.0 || value.toDouble() > 180.0)
		{
			rw::rqwu::MessageBox::warning(this, "警告", "请输入0~180范围的角度!");
		}
		auto& processParam = Modules::getInstance().configManagerModule.processParams;
		processParam[1].rotateAngle = value.toDouble();
		ui->btn_RotateAngleB->setText(value);
	}
}

void Dlg_OffSet::ckb_usingA_checked(bool ischecked)
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[0].enabled = ischecked;
}

void Dlg_OffSet::ckb_usingB_checked(bool ischecked)
{
	auto& processParam = Modules::getInstance().configManagerModule.processParams;
	processParam[1].enabled = ischecked;
}

