#pragma once

#include <QDialog>

class QPushButton;

namespace ui
{
	/// <summary>
	/// 模型参数编辑对话框（偏移量 + 匹配参数）。
	/// 提供中心点1/中心点2 两组独立的 OffsetX / OffsetY / OffsetAngle，
	/// 以及共用的 NumMatches / MinScore / MinAngle / MaxAngle 匹配参数字段，
	/// 点击 QPushButton 弹出 NumberKeyboard 输入。
	/// 六个偏移字段两侧配有 +/- 步进按钮（固定步长 0.1）。
	/// </summary>
	class OffsetEditorDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit OffsetEditorDialog(const QString& modelName,
			double offsetX, double offsetY, double offsetAngle,
			double offset2X, double offset2Y, double offset2Angle,
			int numMatches, double minScore,
			double minAngleDeg, double maxAngleDeg,
			QWidget* parent = nullptr);

		double offsetX() const { return offsetX_; }
		double offsetY() const { return offsetY_; }
		double offsetAngle() const { return offsetAngle_; }
		double offset2X() const { return offset2X_; }
		double offset2Y() const { return offset2Y_; }
		double offset2Angle() const { return offset2Angle_; }
		int numMatches() const { return numMatches_; }
		double minScore() const { return minScore_; }
		double minAngleDeg() const { return minAngleDeg_; }
		double maxAngleDeg() const { return maxAngleDeg_; }

	private slots:
		void onOffsetXClicked();
		void onOffsetYClicked();
		void onOffsetAngleClicked();
		void onStepOffsetX(bool plus);
		void onStepOffsetY(bool plus);
		void onStepOffsetAngle(bool plus);
		void onOffset2XClicked();
		void onOffset2YClicked();
		void onOffset2AngleClicked();
		void onStepOffset2X(bool plus);
		void onStepOffset2Y(bool plus);
		void onStepOffset2Angle(bool plus);
		void onNumMatchesClicked();
		void onMinScoreClicked();
		void onMinAngleClicked();
		void onMaxAngleClicked();
		void onOK();
		void onCancel();

	private:
		void buildUI();
		bool inputDoubleParam(QPushButton* button, double& value,
			double min, double max, int decimals);
		bool inputIntParam(QPushButton* button, int& value, int min, int max);
		/// <summary>步进偏移量：value += delta * kStep，钳位到 [min, max]，更新按钮文本。</summary>
		void applyStep(QPushButton* valueBtn, double& value,
			double delta, double min, double max, int decimals);

		static constexpr double kStep = 0.1;

		// 数值按钮
		QPushButton* btnOffsetX_{ nullptr };
		QPushButton* btnOffsetY_{ nullptr };
		QPushButton* btnOffsetAngle_{ nullptr };
		QPushButton* btnOffset2X_{ nullptr };
		QPushButton* btnOffset2Y_{ nullptr };
		QPushButton* btnOffset2Angle_{ nullptr };
		QPushButton* btnNumMatches_{ nullptr };
		QPushButton* btnMinScore_{ nullptr };
		QPushButton* btnMinAngle_{ nullptr };
		QPushButton* btnMaxAngle_{ nullptr };

		// +/- 步进按钮（仅两组偏移字段有）
		QPushButton* btnMinusX_{ nullptr };
		QPushButton* btnPlusX_{ nullptr };
		QPushButton* btnMinusY_{ nullptr };
		QPushButton* btnPlusY_{ nullptr };
		QPushButton* btnMinusAngle_{ nullptr };
		QPushButton* btnPlusAngle_{ nullptr };
		QPushButton* btnMinus2X_{ nullptr };
		QPushButton* btnPlus2X_{ nullptr };
		QPushButton* btnMinus2Y_{ nullptr };
		QPushButton* btnPlus2Y_{ nullptr };
		QPushButton* btnMinus2Angle_{ nullptr };
		QPushButton* btnPlus2Angle_{ nullptr };

		QString modelName_;
		double offsetX_{ 0.0 };
		double offsetY_{ 0.0 };
		double offsetAngle_{ 0.0 };
		double offset2X_{ 0.0 };
		double offset2Y_{ 0.0 };
		double offset2Angle_{ 0.0 };
		int numMatches_{ 1 };
		double minScore_{ 0.5 };
		double minAngleDeg_{ -45.0 };
		double maxAngleDeg_{ 45.0 };
	};
}
