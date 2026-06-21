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
#include <QHBoxLayout>

#include <algorithm>
#include <set>

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

		// 默认按创建时间降序排列
		ui->cbox_sort->setCurrentIndex(3);

		fullKeyboard_ = new rw::rqwu::FullKeyboard(this);
		fullKeyboard_->emptyInputPolicy = rw::rqwu::Keyboard::EmptyInputPolicy::EnableAndAccept;

		// === 多选支持 ===
		ui->listView_modelList->setSelectionMode(QAbstractItemView::ExtendedSelection);

		ui->listView_modelList->setModel(listModel_);

		// 表头配置
		ui->tableWidget_modelInfo->horizontalHeader()->setVisible(false);
		ui->tableWidget_modelInfo->verticalHeader()->setVisible(false);
		ui->tableWidget_modelInfo->setEditTriggers(QAbstractItemView::NoEditTriggers);
		ui->tableWidget_modelInfo->setSelectionMode(QAbstractItemView::NoSelection);
		ui->tableWidget_modelInfo->setFocusPolicy(Qt::NoFocus);

		// === 重建预览区域：替换单图 QLabel 为两列布局（原图 / 模板图）===
		ui->vLayout_preview->removeWidget(ui->label_imgPreview);
		ui->label_imgPreview->hide();

		auto* hPreviews = new QHBoxLayout();
		ui->vLayout_preview->addLayout(hPreviews);

		auto createPreviewColumn = [this, hPreviews](const QString& title, QLabel*& outImageLabel) {
			auto* vCol = new QVBoxLayout();

			auto* titleLabel = new QLabel(title, this);
			titleLabel->setStyleSheet(QStringLiteral(
				"font-size: 16px; font-weight: bold; color: rgb(85, 85, 85);"));
			titleLabel->setAlignment(Qt::AlignCenter);

			auto* imgLabel = new QLabel(this);
			imgLabel->setMinimumSize(200, 150);
			imgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			imgLabel->setAlignment(Qt::AlignCenter);
			outImageLabel = imgLabel;

			vCol->addWidget(titleLabel);
			vCol->addWidget(imgLabel);
			hPreviews->addLayout(vCol);
		};

		createPreviewColumn(QStringLiteral("原图"),   labelImgOriginal_);
		createPreviewColumn(QStringLiteral("模板图"), labelImgTemplate_);

		// 插入到右侧操作区域（vLayout_actions 之上）
		auto* vLayout = ui->vLayout_detail;
		constexpr int kInsertPos = 2;  // 在 gBox_imgPreview(0) 和 tableWidget(1) 之后、操作按钮(2) 之前

		labelLoadedStatus_ = new QLabel(this);
		labelLoadedStatus_->setStyleSheet(
			"QLabel { font-size: 18px; font-weight: bold; color: #2196F3; padding: 4px 0; }");
		labelLoadedStatus_->setText(QStringLiteral("已加载: 0 个"));

		pbtnUnloadAll_ = new QPushButton(QStringLiteral("全部卸载"), this);
		pbtnUnloadAll_->setStyleSheet(ui->pbtn_loadModel->styleSheet());
		pbtnUnloadAll_->setMinimumHeight(44);

		auto* statusBarLayout = new QHBoxLayout();
		statusBarLayout->addWidget(labelLoadedStatus_);
		statusBarLayout->addStretch();
		statusBarLayout->addWidget(pbtnUnloadAll_);
		vLayout->insertLayout(kInsertPos, statusBarLayout);

		connect(pbtnUnloadAll_, &QPushButton::clicked,
			this, &ModelManagerDialog::onUnloadAll);

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
		refreshLoadedState();

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
				case 0: return a.base_info.name < b.base_info.name;           // 名称升序
				case 3:
				default: return a.getCreateTime() > b.getCreateTime();       // 创建时间降序
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

	void ModelManagerDialog::refreshLoadedState()
	{
		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		const auto ids = bun->getLoadedModelIds();
		std::set<std::string> idSet(ids.begin(), ids.end());
		listModel_->setLoadedModelIds(idSet);

		const int count = static_cast<int>(ids.size());
		labelLoadedStatus_->setText(
			QStringLiteral("已加载: %1 个").arg(count));
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

		// 三图预览：原图 / 标注图 / 模板图
		previewOriginal_.ensure(labelImgOriginal_);
		if (data._originalImage.IsInitialized())
		{
			try { previewOriginal_.display(data._originalImage); }
			catch (...) {}
		}

		previewTemplate_.ensure(labelImgTemplate_);
		if (data._templateMatImage.IsInitialized())
		{
			try { previewTemplate_.display(data._templateMatImage); }
			catch (...) {}
		}

		}
	std::vector<std::string> ModelManagerDialog::selectedModelIds() const
	{
		std::vector<std::string> ids;
		const auto indexes = ui->listView_modelList->selectionModel()->selectedRows();
		for (const auto& idx : indexes)
		{
			if (idx.row() >= 0 && idx.row() < allModels_.size())
				ids.push_back(allModels_.at(idx.row()).getId());
		}
		return ids;
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
		refreshLoadedState();
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
		refreshLoadedState();
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
		refreshLoadedState();
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
		const auto ids = selectedModelIds();
		if (ids.empty())
		{
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("提示"), QStringLiteral("请先在列表中选择一个或多个模型"));
			return;
		}

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		std::vector<std::string> failedIds;
		std::string err;
		const bool allOk = bun->loadModels(ids, &failedIds, &err);

		const int successCount = static_cast<int>(ids.size()) - static_cast<int>(failedIds.size());
		QString msg = QStringLiteral("成功加载 %1 个模型").arg(successCount);
		if (!failedIds.empty())
			msg += QStringLiteral("，%1 个失败").arg(static_cast<int>(failedIds.size()));

		if (allOk)
			rw::rqwu::MessageBox::information(this, QStringLiteral("加载模型"), msg);
		else
			rw::rqwu::MessageBox::warning(this, QStringLiteral("加载模型"), msg);

		refreshLoadedState();
	}

	void ModelManagerDialog::onUnloadAll()
	{
		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		bun->unloadAllModels();
		refreshLoadedState();
	}

	void ModelManagerDialog::onRenameModel()
	{
		const auto ids = selectedModelIds();
		if (ids.empty())
			return;

		// 重命名仅对焦点项生效
		const std::string id = allModels_.at(selectedRow()).getId();

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
			refreshLoadedState();
		}
		else
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("重命名失败"), QString::fromStdString(err));
		}
	}

	void ModelManagerDialog::onDeleteModel()
	{
		const auto ids = selectedModelIds();
		if (ids.empty())
			return;

		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		// 多选确认
		const int count = static_cast<int>(ids.size());
		QString confirmMsg;
		if (count == 1)
		{
			const QString name = QString::fromStdString(allModels_.at(selectedRow()).base_info.name);
			confirmMsg = QStringLiteral("确定要删除模型 \"%1\" 吗？").arg(name);
		}
		else
		{
			confirmMsg = QStringLiteral("确定要删除选中的 %1 个模型吗？").arg(count);
		}

		if (rw::rqwu::MessageBox::question(this,
			QStringLiteral("确认删除"), confirmMsg)
			!= rw::rqwu::MessageBox::StandardButton::Yes)
			return;

		int successCount = 0;
		int failCount = 0;
		for (const auto& id : ids)
		{
			std::string err;
			if (bun->deleteModel(id, &err))
				++successCount;
			else
				++failCount;
		}

		refreshModelList();
		refreshLoadedState();
		if (listModel_->modelCount() > 0)
		{
			ui->listView_modelList->setCurrentIndex(listModel_->index(0, 0));
			refreshModelDetail(0);
		}

		if (failCount > 0)
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("删除结果"),
				QStringLiteral("成功 %1 个，失败 %2 个").arg(successCount).arg(failCount));
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
		const auto ids = selectedModelIds();
		if (ids.empty())
		{
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("提示"), QStringLiteral("请先选择一个模型"));
			return;
		}

		// 编辑仅对焦点项生效
		const std::string id = allModels_.at(selectedRow()).getId();
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
