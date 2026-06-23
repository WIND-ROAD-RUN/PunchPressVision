#include "UI/OffsetEditorDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <QtMath>

#include <rwul/rqwu/Keyboard/rqwu_NumberKeyboard.h>

namespace ui
{
	OffsetEditorDialog::OffsetEditorDialog(const QString& modelName,
		double offsetX, double offsetY, double offsetAngle,
		int numMatches, double minScore,
		double minAngleDeg, double maxAngleDeg,
		QWidget* parent)
		: QDialog(parent)
		, modelName_(modelName)
		, offsetX_(offsetX)
		, offsetY_(offsetY)
		, offsetAngle_(offsetAngle)
		, numMatches_(numMatches)
		, minScore_(minScore)
		, minAngleDeg_(minAngleDeg)
		, maxAngleDeg_(maxAngleDeg)
	{
		setWindowTitle(QStringLiteral("设置参数 - %1").arg(modelName_));
		setMinimumWidth(520);

#ifdef PPV_RELEASE_FULLSCREEN
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
#endif

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

		// 数值按钮样式（统一）
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

		// +/- 按钮样式（紧凑方形）
		const QString stepBtnStyle = QStringLiteral(
			"QPushButton {"
			"  font-size: 20px; font-weight: bold;"
			"  border: 2px solid #CCC;"
			"  border-radius: 4px;"
			"  background-color: #F0F0F0;"
			"  color: #444;"
			"  min-width: 44px; min-height: 44px;"
			"  max-width: 44px; max-height: 44px;"
			"}"
			"QPushButton:hover {"
			"  border-color: #2196F3;"
			"  background-color: #E3F2FD;"
			"}"
			"QPushButton:pressed {"
			"  background-color: #BBDEFB;"
			"}"
		);

		// 辅助：创建一行 [−] [value] [+]
		auto createStepRow = [&](const QString& labelText,
			QPushButton*& valueBtn, QPushButton*& minusBtn, QPushButton*& plusBtn,
			const QString& tooltip, double initVal, int decimals)
		{
			auto* hRow = new QHBoxLayout();
			hRow->setSpacing(4);

			minusBtn = new QPushButton(QStringLiteral("\xe2\x88\x92"), this);  // U+2212 MINUS SIGN
			minusBtn->setStyleSheet(stepBtnStyle);
			minusBtn->setToolTip(QStringLiteral("减小 0.1"));
			hRow->addWidget(minusBtn);

			valueBtn = new QPushButton(QString::number(initVal, 'f', decimals), this);
			valueBtn->setStyleSheet(btnStyle);
			valueBtn->setToolTip(tooltip);
			hRow->addWidget(valueBtn);

			plusBtn = new QPushButton(QStringLiteral("+"), this);
			plusBtn->setStyleSheet(stepBtnStyle);
			plusBtn->setToolTip(QStringLiteral("增大 0.1"));
			hRow->addWidget(plusBtn);

			formLayout->addRow(labelText, hRow);
		};

		// ---- 偏移量区域（带 +/- 步进） ----

		createStepRow(QStringLiteral("左右偏移 (mm):"),
			btnOffsetX_, btnMinusX_, btnPlusX_,
			QStringLiteral("左右偏移 (mm)，范围 -9999.999 ~ 9999.999"),
			offsetX_, 3);
		connect(btnOffsetX_, &QPushButton::clicked, this, &OffsetEditorDialog::onOffsetXClicked);
		connect(btnMinusX_, &QPushButton::clicked, this, [this]() { onStepOffsetX(false); });
		connect(btnPlusX_, &QPushButton::clicked, this, [this]() { onStepOffsetX(true); });

		createStepRow(QStringLiteral("上下偏移 (mm):"),
			btnOffsetY_, btnMinusY_, btnPlusY_,
			QStringLiteral("上下偏移 (mm)，范围 -9999.999 ~ 9999.999"),
			offsetY_, 3);
		connect(btnOffsetY_, &QPushButton::clicked, this, &OffsetEditorDialog::onOffsetYClicked);
		connect(btnMinusY_, &QPushButton::clicked, this, [this]() { onStepOffsetY(false); });
		connect(btnPlusY_, &QPushButton::clicked, this, [this]() { onStepOffsetY(true); });

		createStepRow(QStringLiteral("角度偏移 (\xc2\xb0):"),
			btnOffsetAngle_, btnMinusAngle_, btnPlusAngle_,
			QStringLiteral("角度偏移 (°)，范围 -360.0 ~ 360.0"),
			offsetAngle_, 3);
		connect(btnOffsetAngle_, &QPushButton::clicked, this, &OffsetEditorDialog::onOffsetAngleClicked);
		connect(btnMinusAngle_, &QPushButton::clicked, this, [this]() { onStepOffsetAngle(false); });
		connect(btnPlusAngle_, &QPushButton::clicked, this, [this]() { onStepOffsetAngle(true); });

		// ---- 匹配参数分隔线 ----
		auto* sepLabel = new QLabel(QStringLiteral("\xe2\x94\x80\xe2\x94\x80 匹配参数 \xe2\x94\x80\xe2\x94\x80"), this);
		sepLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #2196F3; padding: 12px 0 4px 0;");
		sepLabel->setAlignment(Qt::AlignCenter);
		formLayout->addRow(sepLabel);

		// 查找数量
		btnNumMatches_ = new QPushButton(QString::number(numMatches_), this);
		btnNumMatches_->setStyleSheet(btnStyle);
		btnNumMatches_->setToolTip(QStringLiteral("模板匹配查找数量 (1~20)"));
		connect(btnNumMatches_, &QPushButton::clicked, this, &OffsetEditorDialog::onNumMatchesClicked);
		formLayout->addRow(QStringLiteral("查找数量:"), btnNumMatches_);

		// 最低匹配分数
		btnMinScore_ = new QPushButton(QString::number(minScore_, 'f', 2), this);
		btnMinScore_->setStyleSheet(btnStyle);
		btnMinScore_->setToolTip(QStringLiteral("最低匹配分数 (0.00 ~ 1.00)"));
		connect(btnMinScore_, &QPushButton::clicked, this, &OffsetEditorDialog::onMinScoreClicked);
		formLayout->addRow(QStringLiteral("最低分数:"), btnMinScore_);

		// 最低角度（度）
		btnMinAngle_ = new QPushButton(QString::number(minAngleDeg_, 'f', 1), this);
		btnMinAngle_->setStyleSheet(btnStyle);
		btnMinAngle_->setToolTip(QStringLiteral("角度搜索范围下限 (°)，范围 -360.0 ~ 360.0"));
		connect(btnMinAngle_, &QPushButton::clicked, this, &OffsetEditorDialog::onMinAngleClicked);
		formLayout->addRow(QStringLiteral("最低角度 (\xc2\xb0):"), btnMinAngle_);

		// 最高角度（度）
		btnMaxAngle_ = new QPushButton(QString::number(maxAngleDeg_, 'f', 1), this);
		btnMaxAngle_->setStyleSheet(btnStyle);
		btnMaxAngle_->setToolTip(QStringLiteral("角度搜索范围上限 (°)，范围 -360.0 ~ 360.0"));
		connect(btnMaxAngle_, &QPushButton::clicked, this, &OffsetEditorDialog::onMaxAngleClicked);
		formLayout->addRow(QStringLiteral("最高角度 (\xc2\xb0):"), btnMaxAngle_);

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

	void OffsetEditorDialog::onStepOffsetX(bool plus)
	{
		applyStep(btnOffsetX_, offsetX_, plus ? 1.0 : -1.0, -9999.999, 9999.999, 3);
	}

	void OffsetEditorDialog::onStepOffsetY(bool plus)
	{
		applyStep(btnOffsetY_, offsetY_, plus ? 1.0 : -1.0, -9999.999, 9999.999, 3);
	}

	void OffsetEditorDialog::onStepOffsetAngle(bool plus)
	{
		applyStep(btnOffsetAngle_, offsetAngle_, plus ? 1.0 : -1.0, -360.0, 360.0, 3);
	}

	void OffsetEditorDialog::onOK()
	{
		accept();
	}

	void OffsetEditorDialog::onCancel()
	{
		reject();
	}

	void OffsetEditorDialog::applyStep(QPushButton* valueBtn, double& value,
		double delta, double min, double max, int decimals)
	{
		double newValue = value + delta * kStep;

		// 钳位
		if (newValue < min) newValue = min;
		if (newValue > max) newValue = max;

		// 按小数位数舍入，避免浮点累积误差
		const double scale = qPow(10.0, decimals);
		newValue = qRound(newValue * scale) / scale;

		if (qFuzzyCompare(newValue, value))
			return;

		value = newValue;
		valueBtn->setText(QString::number(value, 'f', decimals));
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

	bool OffsetEditorDialog::inputIntParam(QPushButton* button, int& value, int min, int max)
	{
		QString input = QString::number(value);

		rw::rqwu::NumberKeyboard::InputDataConfig cfg;
		cfg.isUsingMin = true;
		cfg.isUsingMax = true;
		cfg.min = min;
		cfg.max = max;

		const auto result = rw::rqwu::NumberKeyboard::inputDataOnQPushButton(button, input, cfg);
		if (result != rw::rqwu::NumberKeyboard::Accept)
			return false;

		bool ok = false;
		const int newValue = input.toInt(&ok);
		if (!ok)
			return false;

		value = newValue;
		button->setText(QString::number(value));
		return true;
	}

	void OffsetEditorDialog::onNumMatchesClicked()
	{
		inputIntParam(btnNumMatches_, numMatches_, 1, 20);
	}

	void OffsetEditorDialog::onMinScoreClicked()
	{
		inputDoubleParam(btnMinScore_, minScore_, 0.0, 1.0, 2);
	}

	void OffsetEditorDialog::onMinAngleClicked()
	{
		if (inputDoubleParam(btnMinAngle_, minAngleDeg_, -360.0, 360.0, 1))
		{
			// 确保最低角度 ≤ 最高角度
			if (minAngleDeg_ > maxAngleDeg_)
			{
				maxAngleDeg_ = minAngleDeg_;
				btnMaxAngle_->setText(QString::number(maxAngleDeg_, 'f', 1));
			}
		}
	}

	void OffsetEditorDialog::onMaxAngleClicked()
	{
		if (inputDoubleParam(btnMaxAngle_, maxAngleDeg_, -360.0, 360.0, 1))
		{
			// 确保最高角度 ≥ 最低角度
			if (maxAngleDeg_ < minAngleDeg_)
			{
				minAngleDeg_ = maxAngleDeg_;
				btnMinAngle_->setText(QString::number(minAngleDeg_, 'f', 1));
			}
		}
	}
}
