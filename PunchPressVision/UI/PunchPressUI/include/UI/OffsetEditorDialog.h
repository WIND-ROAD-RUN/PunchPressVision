#pragma once

#include <QDialog>

class QPushButton;

namespace ui
{
	/// <summary>
	/// 模型参数编辑对话框（偏移量 + 匹配参数）。
	/// 提供 OffsetX / OffsetY / OffsetAngle / NumMatches / MinScore / MinAngle / MaxAngle
	/// 七个数值字段，点击 QPushButton 弹出 NumberKeyboard 输入。
	/// </summary>
	class OffsetEditorDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit OffsetEditorDialog(const QString& modelName,
			double offsetX, double offsetY, double offsetAngle,
			int numMatches, double minScore,
			double minAngleDeg, double maxAngleDeg,
			QWidget* parent = nullptr);

		double offsetX() const { return offsetX_; }
		double offsetY() const { return offsetY_; }
		double offsetAngle() const { return offsetAngle_; }
		int numMatches() const { return numMatches_; }
		double minScore() const { return minScore_; }
		double minAngleDeg() const { return minAngleDeg_; }
		double maxAngleDeg() const { return maxAngleDeg_; }

	private slots:
		void onOffsetXClicked();
		void onOffsetYClicked();
		void onOffsetAngleClicked();
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

		QPushButton* btnOffsetX_{ nullptr };
		QPushButton* btnOffsetY_{ nullptr };
		QPushButton* btnOffsetAngle_{ nullptr };
		QPushButton* btnNumMatches_{ nullptr };
		QPushButton* btnMinScore_{ nullptr };
		QPushButton* btnMinAngle_{ nullptr };
		QPushButton* btnMaxAngle_{ nullptr };

		QString modelName_;
		double offsetX_{ 0.0 };
		double offsetY_{ 0.0 };
		double offsetAngle_{ 0.0 };
		int numMatches_{ 1 };
		double minScore_{ 0.5 };
		double minAngleDeg_{ -45.0 };
		double maxAngleDeg_{ 45.0 };
	};
}
