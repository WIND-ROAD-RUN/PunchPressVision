#pragma once

#include <QDialog>
#include <QPushButton>

#include "halconcpp/HalconCpp.h"
#include "UI/HalconInteractiveLabel.h"
#include "UI/StampPatternListModel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DlgStampPatternPickerClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }

namespace ui
{
	/// <summary>
	/// 套版选择对话框（供创建/修改模型时选择本次使用的套版）。
	/// 左侧套版列表，右侧预览；双击列表项直接选定。
	/// exec() 返回 Accepted 后，用 selectedPatternId() 取结果（空 = 不使用套版）。
	/// </summary>
	class StampPatternPickerDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit StampPatternPickerDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~StampPatternPickerDialog() override;

		/// <summary>设置打开时预选中的套版 id（空 = 选中第一项）。</summary>
		void setInitialPatternId(const std::string& id) { initialId_ = id; }
		/// <summary>用户确认后：选中的套版 id（空 = 不使用套版）。</summary>
		std::string selectedPatternId() const { return selectedId_; }

	protected:
		void showEvent(QShowEvent* event) override;

	private slots:
		void onListSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
		void onListDoubleClicked(const QModelIndex& index);
		void onUse();
		void onUseNone();
		void onCancel();

	private:
		void buildConnections();
		void refreshPatternList();
		void refreshPreview(int row);
		int selectedRow() const;
		void acceptWithId(const std::string& id);

		Ui::DlgStampPatternPickerClass* ui;
		app::PunchPressApp& app_;
		StampPatternListModel* listModel_;

		// 预览控件（动态创建，替换 .ui 中的 QLabel 占位）
		HalconInteractiveLabel* labelImgPreview_{ nullptr };

		QVector<Config::StampPatternInfo> allPatterns_;
		std::string initialId_;
		std::string selectedId_;
	};
}
