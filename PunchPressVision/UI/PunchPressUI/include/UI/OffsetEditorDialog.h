#pragma once

#include <QDialog>

class QPushButton;

namespace ui
{
	/// <summary>
	/// 模型偏移量编辑对话框。
	/// 提供 OffsetX / OffsetY / OffsetAngle 三个数值字段，
	/// 点击 QPushButton 弹出 NumberKeyboard 输入。
	/// </summary>
	class OffsetEditorDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit OffsetEditorDialog(const QString& modelName,
			double offsetX, double offsetY, double offsetAngle,
			QWidget* parent = nullptr);

		double offsetX() const { return offsetX_; }
		double offsetY() const { return offsetY_; }
		double offsetAngle() const { return offsetAngle_; }

	private slots:
		void onOffsetXClicked();
		void onOffsetYClicked();
		void onOffsetAngleClicked();
		void onOK();
		void onCancel();

	private:
		void buildUI();
		bool inputDoubleParam(QPushButton* button, double& value,
			double min, double max, int decimals);

		QPushButton* btnOffsetX_{ nullptr };
		QPushButton* btnOffsetY_{ nullptr };
		QPushButton* btnOffsetAngle_{ nullptr };

		QString modelName_;
		double offsetX_{ 0.0 };
		double offsetY_{ 0.0 };
		double offsetAngle_{ 0.0 };
	};
}
