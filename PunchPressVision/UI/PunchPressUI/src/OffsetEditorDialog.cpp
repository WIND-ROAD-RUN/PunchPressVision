#include "UI/OffsetEditorDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <rwul/rqwu/Keyboard/rqwu_NumberKeyboard.h>

namespace ui
{
	OffsetEditorDialog::OffsetEditorDialog(const QString& modelName,
		double offsetX, double offsetY, double offsetAngle,
		QWidget* parent)
		: QDialog(parent)
		, modelName_(modelName)
		, offsetX_(offsetX)
		, offsetY_(offsetY)
		, offsetAngle_(offsetAngle)
	{
		setWindowTitle(QStringLiteral("设置偏移量 - %1").arg(modelName_));
		setMinimumWidth(400);
		buildUI();
	}

	void OffsetEditorDialog::buildUI()
	{
		auto* rootLayout = new QVBoxLayout(this);

		// 说明标签
		auto* label = new QLabel(QStringLiteral("请设置模型 \"%1\" 的偏移量：").arg(modelName_));
		label->setStyleSheet("font-size: 16px; font-weight: bold; color: #2c3e50; padding: 8px 0;");
		rootLayout->addWidget(label);

		// 表单区域
		auto* formGroup = new QGroupBox(this);
		formGroup->setStyleSheet(QStringLiteral(
			"QGroupBox {"
			"  font-size: 14px;"
			"  background-color: #F8F8F8;"
			"  border: 1px solid #DDD;"
			"  border-radius: 8px;"
			"  padding: 12px;"
			"}"));
		auto* formLayout = new QFormLayout(formGroup);
		formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
		formLayout->setSpacing(12);

		// 按钮样式（统一）
		const QString btnStyle = QStringLiteral(
			"QPushButton {"
			"  font-size: 18px;"
			"  padding: 8px 16px;"
			"  border: 2px solid #CCC;"
			"  border-radius: 4px;"
			"  background-color: white;"
			"  color: #444;"
			"  min-width: 120px;"
			"}"
			"QPushButton:hover {"
			"  border-color: #2196F3;"
			"  background-color: #E3F2FD;"
			"}");

		// OffsetX
		btnOffsetX_ = new QPushButton(QString::number(offsetX_, 'f', 3), this);
		btnOffsetX_->setStyleSheet(btnStyle);
		btnOffsetX_->setToolTip(QStringLiteral("X 方向偏移 (mm)，范围 -9999.999 ~ 9999.999"));
		connect(btnOffsetX_, &QPushButton::clicked, this, &OffsetEditorDialog::onOffsetXClicked);
		formLayout->addRow(QStringLiteral("OffsetX (mm):"), btnOffsetX_);

		// OffsetY
		btnOffsetY_ = new QPushButton(QString::number(offsetY_, 'f', 3), this);
		btnOffsetY_->setStyleSheet(btnStyle);
		btnOffsetY_->setToolTip(QStringLiteral("Y 方向偏移 (mm)，范围 -9999.999 ~ 9999.999"));
		connect(btnOffsetY_, &QPushButton::clicked, this, &OffsetEditorDialog::onOffsetYClicked);
		formLayout->addRow(QStringLiteral("OffsetY (mm):"), btnOffsetY_);

		// OffsetAngle
		btnOffsetAngle_ = new QPushButton(QString::number(offsetAngle_, 'f', 3), this);
		btnOffsetAngle_->setStyleSheet(btnStyle);
		btnOffsetAngle_->setToolTip(QStringLiteral("角度偏移 (°)，范围 -360.0 ~ 360.0"));
		connect(btnOffsetAngle_, &QPushButton::clicked, this, &OffsetEditorDialog::onOffsetAngleClicked);
		formLayout->addRow(QStringLiteral("OffsetAngle (°):"), btnOffsetAngle_);

		rootLayout->addWidget(formGroup);
		rootLayout->addStretch();

		// 确定/取消按钮
		auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		btnBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("确定"));
		btnBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
		btnBox->button(QDialogButtonBox::Ok)->setStyleSheet(
			"QPushButton { font-size: 18px; padding: 8px 24px; font-weight: bold; }");
		btnBox->button(QDialogButtonBox::Cancel)->setStyleSheet(
			"QPushButton { font-size: 18px; padding: 8px 24px; }");
		connect(btnBox, &QDialogButtonBox::accepted, this, &OffsetEditorDialog::onOK);
		connect(btnBox, &QDialogButtonBox::rejected, this, &OffsetEditorDialog::onCancel);
		rootLayout->addWidget(btnBox);
	}

	void OffsetEditorDialog::onOffsetXClicked()
	{
		inputDoubleParam(btnOffsetX_, offsetX_, -9999.999, 9999.999, 3);
	}

	void OffsetEditorDialog::onOffsetYClicked()
	{
		inputDoubleParam(btnOffsetY_, offsetY_, -9999.999, 9999.999, 3);
	}

	void OffsetEditorDialog::onOffsetAngleClicked()
	{
		inputDoubleParam(btnOffsetAngle_, offsetAngle_, -360.0, 360.0, 3);
	}

	void OffsetEditorDialog::onOK()
	{
		accept();
	}

	void OffsetEditorDialog::onCancel()
	{
		reject();
	}

	bool OffsetEditorDialog::inputDoubleParam(QPushButton* button, double& value,
		double min, double max, int decimals)
	{
		QString input = QString::number(value, 'f', decimals);

		rw::rqwu::NumberKeyboard::InputDataConfig cfg;
		cfg.isUsingMin = true;
		cfg.isUsingMax = true;
		cfg.min = min;
		cfg.max = max;

		const auto result = rw::rqwu::NumberKeyboard::inputDataOnQPushButton(button, input, cfg);
		if (result != rw::rqwu::NumberKeyboard::Accept)
			return false;

		bool ok = false;
		const double newValue = input.toDouble(&ok);
		if (!ok)
			return false;

		value = newValue;
		button->setText(QString::number(value, 'f', decimals));
		return true;
	}
}
