#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include "halconcpp/HalconCpp.h"
#include "UI/HalconInteractiveLabel.h"
#include "UI/StampPatternListModel.h"

QT_BEGIN_NAMESPACE
namespace Ui { class DlgStampPatternManagerClass; }
QT_END_NAMESPACE

namespace app { class PunchPressApp; }
namespace rw { namespace rqwu { class FullKeyboard; } }

namespace ui
{
	/// <summary>
	/// 套版（StampPattern）图库管理对话框。
	/// 左侧套版列表，右侧预览 + 详情 + 操作。
	/// 支持导入 CAD DXF 图纸与自定义图片、重命名、删除。
	/// </summary>
	class StampPatternManagerDialog : public QDialog
	{
		Q_OBJECT

	public:
		explicit StampPatternManagerDialog(app::PunchPressApp& app, QWidget* parent = nullptr);
		~StampPatternManagerDialog() override;

	protected:
		void showEvent(QShowEvent* event) override;

	private slots:
		void onListSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
		void onImport();
		void onRename();
		void onDelete();
		void onExit();

	private:
		void buildConnections();
		void refreshPatternList();
		void refreshPatternDetail(int row);
		int selectedRow() const;

		Ui::DlgStampPatternManagerClass* ui;
		app::PunchPressApp& app_;
		StampPatternListModel* listModel_;
		rw::rqwu::FullKeyboard* fullKeyboard_;

		// 预览控件（动态创建，替换 .ui 中的 QLabel 占位）
		HalconInteractiveLabel* labelImgPreview_{ nullptr };

		QVector<Config::StampPatternInfo> allPatterns_;
	};
}
