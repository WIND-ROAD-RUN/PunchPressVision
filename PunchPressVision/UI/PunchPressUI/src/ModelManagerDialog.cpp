#include "UI/ModelManagerDialog.h"

#include <algorithm>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QInputDialog>

#include <rwul/rqwu/rqwu_MessageBox.h>

#include "app/PunchPressApp.hpp"

// Win32 windows.h 的 MessageBox 宏需在所有包含后取消
#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	ModelManagerDialog::ModelManagerDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, app_(app)
	{
		setWindowTitle(QStringLiteral("模型库管理"));
		resize(720, 480);
		buildUi();
		refreshModelList();
	}

	ModelManagerDialog::~ModelManagerDialog() = default;

	void ModelManagerDialog::buildUi()
	{
		auto* root = new QVBoxLayout(this);

		auto* topBar = new QHBoxLayout();
		topBar->addWidget(new QLabel(QStringLiteral("搜索:"), this));
		searchEdit_ = new QLineEdit(this);
		topBar->addWidget(searchEdit_, 1);
		topBar->addWidget(new QLabel(QStringLiteral("排序:"), this));
		sortCombo_ = new QComboBox(this);
		sortCombo_->addItems({ QStringLiteral("名称升序"), QStringLiteral("名称降序"),
			QStringLiteral("创建时间升序"), QStringLiteral("创建时间降序") });
		topBar->addWidget(sortCombo_);
		root->addLayout(topBar);

		table_ = new QTableWidget(this);
		table_->setColumnCount(3);
		table_->setHorizontalHeaderLabels({ QStringLiteral("名称"), QStringLiteral("创建时间"), QStringLiteral("ID") });
		table_->horizontalHeader()->setStretchLastSection(true);
		table_->setSelectionBehavior(QAbstractItemView::SelectRows);
		table_->setSelectionMode(QAbstractItemView::SingleSelection);
		table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
		root->addWidget(table_, 1);

		auto* btnBar = new QHBoxLayout();
		loadBtn_ = new QPushButton(QStringLiteral("加载"), this);
		renameBtn_ = new QPushButton(QStringLiteral("重命名"), this);
		deleteBtn_ = new QPushButton(QStringLiteral("删除"), this);
		btnBar->addStretch(1);
		btnBar->addWidget(loadBtn_);
		btnBar->addWidget(renameBtn_);
		btnBar->addWidget(deleteBtn_);
		root->addLayout(btnBar);

		connect(searchEdit_, &QLineEdit::textChanged, this, &ModelManagerDialog::onSearchTextChanged);
		connect(sortCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, &ModelManagerDialog::onSortChanged);
		connect(loadBtn_, &QPushButton::clicked, this, &ModelManagerDialog::onLoadModel);
		connect(renameBtn_, &QPushButton::clicked, this, &ModelManagerDialog::onRenameModel);
		connect(deleteBtn_, &QPushButton::clicked, this, &ModelManagerDialog::onDeleteModel);
	}

	void ModelManagerDialog::refreshModelList()
	{
		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun)
			return;

		auto models = biz.shape_mode_manager_bun->searchModels(searchEdit_ ? searchEdit_->text() : QString());

		// 排序
		const int sort = sortCombo_ ? sortCombo_->currentIndex() : 0;
		std::sort(models.begin(), models.end(),
			[sort](const Config::ShapeModelInfo& a, const Config::ShapeModelInfo& b)
			{
				switch (sort)
				{
				case 1: return a.base_info.name > b.base_info.name;
				case 2: return a.getCreateTime() < b.getCreateTime();
				case 3: return a.getCreateTime() > b.getCreateTime();
				case 0:
				default: return a.base_info.name < b.base_info.name;
				}
			});

		table_->setRowCount(static_cast<int>(models.size()));
		for (int i = 0; i < static_cast<int>(models.size()); ++i)
		{
			const auto& m = models[i];
			table_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(m.base_info.name)));
			table_->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(m.getCreateTime())));
			table_->setItem(i, 2, new QTableWidgetItem(QString::fromStdString(m.getId())));
		}
	}

	std::string ModelManagerDialog::selectedModelId() const
	{
		const int row = table_->currentRow();
		if (row < 0 || !table_->item(row, 2))
			return {};
		return table_->item(row, 2)->text().toStdString();
	}

	void ModelManagerDialog::onSearchTextChanged(const QString&)
	{
		refreshModelList();
	}

	void ModelManagerDialog::onSortChanged(int)
	{
		refreshModelList();
	}

	void ModelManagerDialog::onLoadModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
			return;
		std::string err;
		if (app_.business().shape_mode_manager_bun->loadModel(id, &err))
			rw::rqwu::MessageBox::information(this, QStringLiteral("加载模型"), QStringLiteral("模型已加载"));
		else
			rw::rqwu::MessageBox::warning(this, QStringLiteral("加载模型"), QString::fromStdString(err));
	}

	void ModelManagerDialog::onRenameModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
			return;
		bool ok = false;
		const QString name = QInputDialog::getText(this, QStringLiteral("重命名"),
			QStringLiteral("新名称:"), QLineEdit::Normal, QString(), &ok);
		if (!ok || name.isEmpty())
			return;
		std::string err;
		if (app_.business().shape_mode_manager_bun->renameModel(id, name, &err))
			refreshModelList();
		else
			rw::rqwu::MessageBox::warning(this, QStringLiteral("重命名"), QString::fromStdString(err));
	}

	void ModelManagerDialog::onDeleteModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
			return;
		// FR-030: 删除二次确认
		if (rw::rqwu::MessageBox::question(this, QStringLiteral("删除模型"),
			QStringLiteral("确认删除选中的模型？")) != rw::rqwu::MessageBox::StandardButton::Yes)
			return;
		std::string err;
		if (app_.business().shape_mode_manager_bun->deleteModel(id, &err))
			refreshModelList();
		else
			rw::rqwu::MessageBox::warning(this, QStringLiteral("删除模型"), QString::fromStdString(err));
	}
}
