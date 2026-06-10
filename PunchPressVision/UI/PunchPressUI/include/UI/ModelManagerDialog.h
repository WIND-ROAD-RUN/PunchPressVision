#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QTableWidget;
class QLineEdit;
class QComboBox;
class QPushButton;
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

namespace ui
{
	// 模型库管理界面（FR-027、FR-030、FR-031）。新建，代码构建布局。
	class ModelManagerDialog : public QDialog
	{
		Q_OBJECT
	public:
		explicit ModelManagerDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~ModelManagerDialog() override;

	private slots:
		void onSearchTextChanged(const QString& text);
		void onSortChanged(int index);
		void onRenameModel();
		void onLoadModel();
		void onDeleteModel();

	private:
		void buildUi();
		void refreshModelList();
		std::string selectedModelId() const;

		app::PunchPressApp& app_;
		QTableWidget* table_ = nullptr;
		QLineEdit* searchEdit_ = nullptr;
		QComboBox* sortCombo_ = nullptr;
		QPushButton* renameBtn_ = nullptr;
		QPushButton* loadBtn_ = nullptr;
		QPushButton* deleteBtn_ = nullptr;
	};
}
