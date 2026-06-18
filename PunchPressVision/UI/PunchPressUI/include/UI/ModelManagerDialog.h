#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include "halconcpp/HalconCpp.h"
#include "UI/HalconView.h"
#include "UI/ShapeModelListModel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DlgModelManagerClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }
namespace rw { namespace rqwu { class FullKeyboard; } }

namespace ui
{
	/// <summary>
	/// 模型库管理对话框。
	/// 参考 rqwu_DlgModelManager 架构，集成 FullKeyboard 触摸键盘。
	/// 左侧模型列表（搜索/排序/导航），右侧详情面板（预览/元信息/操作）。
	/// 支持多选批量加载模型，已加载模型在列表中蓝色加粗标记。
	/// 支持跳转到 ModelEditorDialog 进行创建和修改。
	/// </summary>
	class ModelManagerDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit ModelManagerDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~ModelManagerDialog() override;

	protected:
		void showEvent(QShowEvent* event) override;

	private slots:
		void onSearchInputClicked();
		void onSearchClicked();
		void onClearClicked();
		void onSortChanged(int index);
		void onListSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
		void onListDoubleClicked(const QModelIndex& index);
		void onPreModel();
		void onNextModel();
		void onLoadModel();
		void onRenameModel();
		void onDeleteModel();
		void onCreateModel();
		void onEditModel();
		void onExit();
		void onUnloadAll();

	private:
		void buildConnections();
		void refreshModelList();
		void refreshModelDetail(int row);
		/// <summary>刷新列表项的已加载标记和状态标签文本。</summary>
		void refreshLoadedState();
		QVector<Config::ShapeModelInfo> filteredAndSortedModels() const;
		/// <summary>返回当前选中的所有模型 ID。</summary>
		std::vector<std::string> selectedModelIds() const;
		int selectedRow() const;

		Ui::DlgModelManagerClass* ui;
		app::PunchPressApp& app_;
		ShapeModelListModel* listModel_;
		rw::rqwu::FullKeyboard* fullKeyboard_;
		HalconView previewOriginal_;
		HalconView previewAnnotated_;
		HalconView previewTemplate_;

		/// 动态创建的预览图像宿主 QLabel（不在 .ui 文件中）
		QLabel* labelImgOriginal_{ nullptr };
		QLabel* labelImgAnnotated_{ nullptr };
		QLabel* labelImgTemplate_{ nullptr };

		/// 动态创建的控件（不在 .ui 文件中）
		QLabel* labelLoadedStatus_{ nullptr };
		QPushButton* pbtnUnloadAll_{ nullptr };

		QVector<Config::ShapeModelInfo> allModels_;
	};
}
