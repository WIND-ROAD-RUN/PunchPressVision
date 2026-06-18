// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>
#include <rwul/rqwu/Keyboard/rqwu_FullKeyboard.h>

#include "UI/ModelManagerDialog.h"
#include "ui_DlgModelManager.h"

#include <QShowEvent>
#include <QPushButton>
#include <QComboBox>
#include <QListView>
#include <QHeaderView>

#include <algorithm>

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"
#include "UI/ModelEditorDialog.h"

#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	ModelManagerDialog::ModelManagerDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::DlgModelManagerClass())
		, app_(app)
		, listModel_(new ShapeModelListModel(this))
	{
		ui->setupUi(this);

		fullKeyboard_ = new rw::rqwu::FullKeyboard(this);
		fullKeyboard_->emptyInputPolicy = rw::rqwu::Keyboard::EmptyInputPolicy::EnableAndAccept;

		ui->listView_modelList->setModel(listModel_);

		// 表头配置
		ui->tableWidget_modelInfo->horizontalHeader()->setVisible(false);
		ui->tableWidget_modelInfo->verticalHeader()->setVisible(false);
		ui->tableWidget_modelInfo->setEditTriggers(QAbstractItemView::NoEditTriggers);
		ui->tableWidget_modelInfo->setSelectionMode(QAbstractItemView::NoSelection);
		ui->tableWidget_modelInfo->setFocusPolicy(Qt::NoFocus);

		buildConnections();
	}

	ModelManagerDialog::~ModelManagerDialog()
	{
		delete ui;
	}

	void ModelManagerDialog::buildConnections()
	{
		// 搜索
		connect(ui->pbtn_searchInput, &QPushButton::clicked,
			this, &ModelManagerDialog::onSearchInputClicked);
		connect(ui->pbtn_search, &QPushButton::clicked,
			this, &ModelManagerDialog::onSearchClicked);
		connect(ui->pbtn_clear, &QPushButton::clicked,
			this, &ModelManagerDialog::onClearClicked);

		// 排序
		connect(ui->cbox_sort, QOverload<int>::of(&QComboBox::currentIndexChanged),
			this, &ModelManagerDialog::onSortChanged);

		// 列表选择 / 双击重命名
		connect(ui->listView_modelList->selectionModel(), &QItemSelectionModel::currentChanged,
			this, &ModelManagerDialog::onListSelectionChanged);
		connect(ui->listView_modelList, &QListView::doubleClicked,
			this, &ModelManagerDialog::onListDoubleClicked);

		// 导航
		connect(ui->pbtn_preModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onPreModel);
		connect(ui->pbtn_nextModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onNextModel);

		// 操作
		connect(ui->pbtn_loadModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onLoadModel);
		connect(ui->pbtn_renameModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onRenameModel);
		connect(ui->pbtn_deleteModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onDeleteModel);
		connect(ui->pbtn_createModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onCreateModel);
		connect(ui->pbtn_editModel, &QPushButton::clicked,
			this, &ModelManagerDialog::onEditModel);
		connect(ui->pbtn_exit, &QPushButton::clicked,
			this, &ModelManagerDialog::onExit);
	}

	void ModelManagerDialog::showEvent(QShowEvent* event)
	{
		QDialog::showEvent(event);
		if (parentWidget())
			resize(parentWidget()->size());
		refreshModelList();

		// 初始选中第一项
		if (listModel_->modelCount() > 0)
		{
			ui->listView_modelList->setCurrentIndex(listModel_->index(0, 0));
			refreshModelDetail(0);
		}
	}

	// ===== 数据 =====

	QVector<Config::ShapeModelInfo> ModelManagerDialog::filteredAndSortedModels() const
	{
		auto& biz = app_.business();
		if (!biz.shape_mode_manager_bun)
			return {};

		const QString keyword = ui->pbtn_searchInput->text();
		QVector<Config::ShapeModelInfo> models;

		if (keyword.isEmpty())
		{
			// 全量
			const auto& raw = biz.shape_mode_manager_bun->getAllModels();
			models = QVector<Config::ShapeModelInfo>(raw.begin(), raw.end());
		}
		else
		{
			const auto& raw = biz.shape_mode_manager_bun->searchModels(keyword);
			models = QVector<Config::ShapeModelInfo>(raw.begin(), raw.end());
		}

		// 排序
		const int sort = ui->cbox_sort->currentIndex();
		std::sort(models.begin(), models.end(),
			[sort](const Config::ShapeModelInfo& a, const Config::ShapeModelInfo& b)
			{
				switch (sort)
				{
				case 1: return a.base_info.name > b.base_info.name;          // 名称降序
				case 2: return a.getCreateTime() < b.getCreateTime();        // 创建时间升序
				case 3: return a.getCreateTime() > b.getCreateTime();        // 创建时间降序
				case 0:
				default: return a.base_info.name < b.base_info.name;         // 名称升序
				}
			});

		return models;
	}

	void ModelManagerDialog::refreshModelList()
	{
		allModels_ = filteredAndSortedModels();
		listModel_->setModelInfos(allModels_);

		// 清空详情
		ui->tableWidget_modelInfo->setRowCount(0);
	}

	static const char* kChannelNames[] = {
		"灰度", "红", "绿", "蓝", "H", "S", "V"
	};

	static QString channelName(int idx)
	{
		if (idx < 0 || idx > 6) return QString::number(idx);
		return QString::fromUtf8(kChannelNames[idx]);
	}

	static QString preprocessSummary(const Config::ShapeModelData& d)
	{
		QStringList parts;
		if (d._createModelUseOpening)
			parts.append(QStringLiteral("开运算(%1)").arg(d._createModelOpeningRadius));
		if (d._createModelUseClosing)
			parts.append(QStringLiteral("闭运算(%1)").arg(d._createModelClosingRadius));
		if (d._createModelUseMean)
			parts.append(QStringLiteral("均值(%1)").arg(d._createModelMeanRadius));
		return parts.isEmpty() ? QStringLiteral("无") : parts.join(QStringLiteral(" + "));
	}

	static QString contrastSummary(const Config::ShapeModelData& d)
	{
		// contrast==0 → auto default
		if (d.contrast == 0)
			return QStringLiteral("自动");
		return QStringLiteral("手动 %1 / %2").arg(d.contrast).arg(d.minContrast);
	}

	void ModelManagerDialog::refreshModelDetail(int row)
	{
		if (row < 0 || row >= allModels_.size())
		{
			ui->tableWidget_modelInfo->setRowCount(0);
			return;
		}

		const auto& info = allModels_.at(row);

		// 从磁盘加载训练参数
		Config::ShapeModelData data;
		data.loadInDir(info.getFolderPath());

		constexpr int kRowCount = 9;
		ui->tableWidget_modelInfo->setRowCount(kRowCount);
		ui->tableWidget_modelInfo->setColumnCount(2);
		ui->tableWidget_modelInfo->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		ui->tableWidget_modelInfo->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
		ui->tableWidget_modelInfo->verticalHeader()->setVisible(true);

		auto setRow = [&](int r, const QString& label, const QString& value)
		{
			auto* keyItem = new QTableWidgetItem(label);
			keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
			auto* valItem = new QTableWidgetItem(value);
			valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
			ui->tableWidget_modelInfo->setItem(r, 0, keyItem);
			ui->tableWidget_modelInfo->setItem(r, 1, valItem);
		};

		int r = 0;
		setRow(r++, QStringLiteral("名称"),     QString::fromStdString(info.base_info.name));
		setRow(r++, QStringLiteral("图像通道"), channelName(data._SingleChannelType));
		setRow(r++, QStringLiteral("预处理"),   preprocessSummary(data));
		setRow(r++, QStringLiteral("对比度"),   contrastSummary(data));
		setRow(r++, QStringLiteral("ROI / 屏蔽"),
			QStringLiteral("%1 / %2")
				.arg(static_cast<int>(data._paintCreateRoiList.size()))
				.arg(static_cast<int>(data._paintShieldRoiList.size())));
		setRow(r++, QStringLiteral("曝光 / 增益"),
			QStringLiteral("%1 / %2")
				.arg(data._createModelExposureTime, 0, 'f', 0)
				.arg(data._createModelGain, 0, 'f', 0));
		setRow(r++, QStringLiteral("创建时间"), QString::fromStdString(info.getCreateTime()));
		setRow(r++, QStringLiteral("更新时间"), QString::fromStdString(info.getUpdateTime()));
		setRow(r++, QStringLiteral("文件夹"),   QString::fromStdString(info.getFolderPath()));

		// 模板预览：尝试显示标注图
		previewView_.ensure(ui->label_imgPreview);
		if (data._annotatedImage.IsInitialized())
		{
			try { previewView_.display(data._annotatedImage); }
			catch (...) {}
		}
		else if (data._originalImage.IsInitialized())
		{
			try { previewView_.display(data._originalImage); }
			catch (...) {}
		}
	}

	std::string ModelManagerDialog::selectedModelId() const
	{
		const int row = selectedRow();
		if (row < 0 || row >= allModels_.size())
			return {};
		return allModels_.at(row).getId();
	}

	int ModelManagerDialog::selectedRow() const
	{
		const auto idx = ui->listView_modelList->currentIndex();
		return idx.isValid() ? idx.row() : -1;
	}

	// ===== 搜索 =====

	void ModelManagerDialog::onSearchInputClicked()
	{
		const int ret = fullKeyboard_->exec();
		if (ret != QDialog::Accepted)
			return;

		const QString text = fullKeyboard_->getValue();
		ui->pbtn_searchInput->setText(text);
	}

	void ModelManagerDialog::onSearchClicked()
	{
		refreshModelList();
		if (listModel_->modelCount() > 0)
		{
			ui->listView_modelList->setCurrentIndex(listModel_->index(0, 0));
			refreshModelDetail(0);
		}
	}

	void ModelManagerDialog::onClearClicked()
	{
		ui->pbtn_searchInput->setText(QString());
		refreshModelList();
		if (listModel_->modelCount() > 0)
		{
			ui->listView_modelList->setCurrentIndex(listModel_->index(0, 0));
			refreshModelDetail(0);
		}
	}

	// ===== 排序 =====

	void ModelManagerDialog::onSortChanged(int /*index*/)
	{
		refreshModelList();
		if (listModel_->modelCount() > 0)
		{
			ui->listView_modelList->setCurrentIndex(listModel_->index(0, 0));
			refreshModelDetail(0);
		}
	}

	// ===== 列表交互 =====

	void ModelManagerDialog::onListSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
	{
		if (!current.isValid())
			return;
		refreshModelDetail(current.row());
	}

	void ModelManagerDialog::onListDoubleClicked(const QModelIndex& index)
	{
		if (!index.isValid())
			return;

		const int ret = fullKeyboard_->exec();
		if (ret != QDialog::Accepted)
			return;

		const QString newName = fullKeyboard_->getValue();
		if (newName.isEmpty())
			return;

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		const std::string id = allModels_.at(index.row()).getId();
		std::string err;
		if (bun->renameModel(id, newName, &err))
		{
			refreshModelList();
		}
		else
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("重命名失败"), QString::fromStdString(err));
		}
	}

	// ===== 导航 =====

	void ModelManagerDialog::onPreModel()
	{
		const int count = listModel_->modelCount();
		if (count == 0)
			return;

		const int cur = selectedRow();
		const int next = (cur - 1 + count) % count;
		ui->listView_modelList->setCurrentIndex(listModel_->index(next, 0));
		refreshModelDetail(next);
	}

	void ModelManagerDialog::onNextModel()
	{
		const int count = listModel_->modelCount();
		if (count == 0)
			return;

		const int cur = selectedRow();
		const int next = (cur + 1) % count;
		ui->listView_modelList->setCurrentIndex(listModel_->index(next, 0));
		refreshModelDetail(next);
	}

	// ===== 操作 =====

	void ModelManagerDialog::onLoadModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
			return;

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		std::string err;
		if (bun->loadModel(id, &err))
		{
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("加载模型"), QStringLiteral("模型已加载"));
		}
		else
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("加载失败"), QString::fromStdString(err));
		}
	}

	void ModelManagerDialog::onRenameModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
			return;

		const int ret = fullKeyboard_->exec();
		if (ret != QDialog::Accepted)
			return;

		const QString newName = fullKeyboard_->getValue();
		if (newName.isEmpty())
			return;

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		std::string err;
		if (bun->renameModel(id, newName, &err))
		{
			refreshModelList();
		}
		else
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("重命名失败"), QString::fromStdString(err));
		}
	}

	void ModelManagerDialog::onDeleteModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
			return;

		const auto name = QString::fromStdString(allModels_.at(selectedRow()).base_info.name);
		if (rw::rqwu::MessageBox::question(this,
			QStringLiteral("确认删除"),
			QStringLiteral("确定要删除模型 \"%1\" 吗？").arg(name))
			!= rw::rqwu::MessageBox::StandardButton::Yes)
			return;

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		std::string err;
		if (bun->deleteModel(id, &err))
		{
			refreshModelList();
			if (listModel_->modelCount() > 0)
			{
				ui->listView_modelList->setCurrentIndex(listModel_->index(0, 0));
				refreshModelDetail(0);
			}
		}
		else
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("删除失败"), QString::fromStdString(err));
		}
	}

	void ModelManagerDialog::onCreateModel()
	{
		ModelEditorDialog dlg(app_, false, {}, this);
		if (dlg.exec() == QDialog::Accepted)
		{
			refreshModelList();
		}
	}

	void ModelManagerDialog::onEditModel()
	{
		const std::string id = selectedModelId();
		if (id.empty())
		{
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("提示"), QStringLiteral("请先选择一个模型"));
			return;
		}
		ModelEditorDialog dlg(app_, true, id, this);
		if (dlg.exec() == QDialog::Accepted)
		{
			refreshModelList();
		}
	}

	void ModelManagerDialog::onExit()
	{
		close();
	}
}
