#pragma once

#include <QDialog>
#include <QLabel>
#include <QProgressBar>

QT_BEGIN_NAMESPACE
namespace Ui { class LoadingDialogClass; };
QT_END_NAMESPACE

class LoadingDialog : public QDialog
{
	Q_OBJECT

public:
	explicit LoadingDialog(QWidget* parent = nullptr);

	void updateMessage(const QString& message);

private:
	QLabel* label;
	QProgressBar* progressBar;

private:
	Ui::LoadingDialogClass* ui;
};
