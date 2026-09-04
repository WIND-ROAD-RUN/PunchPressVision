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
#include <QVBoxLayout>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>

#include <algorithm>
#include <cmath>
#include <set>

#include "app/PunchPressApp.hpp"
#include "Business/ShapeModeManagerBun/ShapeModeManagerBun.hpp"
#include "Business/StampPatternBun/StampPatternBun.hpp"
#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"
#include "infrastructure/StampPatternModule/Config/StampPatternItem.hpp"
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

#ifdef PPV_RELEASE_FULLSCREEN
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setWindowState(windowState() | Qt::WindowFullScreen);
#endif

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

		auto createPreviewColumn = [this, hPreviews](const QString& title, HalconInteractiveLabel*& outImageLabel) {
			auto* vCol = new QVBoxLayout();

			auto* titleLabel = new QLabel(title, this);
			titleLabel->setStyleSheet(QStringLiteral(
				"font-size: 16px; font-weight: bold; color: rgb(85, 85, 85);"));
			titleLabel->setAlignment(Qt::AlignCenter);

			auto* imgLabel = new HalconInteractiveLabel(this);
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

		pbtnPreviewStamp_ = new QPushButton(QStringLiteral("预览套版"), this);
		pbtnPreviewStamp_->setStyleSheet(ui->pbtn_loadModel->styleSheet());
		pbtnPreviewStamp_->setMinimumHeight(44);
		pbtnPreviewStamp_->setEnabled(false);

		auto* statusBarLayout = new QHBoxLayout();
		statusBarLayout->addWidget(labelLoadedStatus_);
		statusBarLayout->addStretch();
		statusBarLayout->addWidget(pbtnPreviewStamp_);
		statusBarLayout->addWidget(pbtnUnloadAll_);
		vLayout->insertLayout(kInsertPos, statusBarLayout);

		connect(pbtnUnloadAll_, &QPushButton::clicked,
			this, &ModelManagerDialog::onUnloadAll);
		connect(pbtnPreviewStamp_, &QPushButton::clicked,
			this, &ModelManagerDialog::onPreviewStamp);

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
			currentStampPatternId_.clear();
			currentStampValid_ = false;
			currentOriginalImage_ = HalconCpp::HImage();
			if (pbtnPreviewStamp_)
				pbtnPreviewStamp_->setEnabled(false);
			return;
		}

		const auto& info = allModels_.at(row);

		// 从磁盘加载训练参数
		Config::ShapeModelData data;
		data.loadInDir(info.getFolderPath());

		// 解析套版关联状态（未使用 / 有效 / 悬空失效）
		currentStampPatternId_ = data.stampPatternId;
		currentOriginalImage_ = data._originalImage;
		currentStampValid_ = false;
		Config::StampPatternItem stampItem;
		if (!currentStampPatternId_.empty())
		{
			auto& stampBun = app_.business().stamp_pattern_bun;
			if (stampBun)
			{
				stampItem = stampBun->getPatternItem(currentStampPatternId_);
				currentStampValid_ = !stampItem.info.getId().empty();
			}
		}
		if (pbtnPreviewStamp_)
			pbtnPreviewStamp_->setEnabled(currentStampValid_);

		constexpr int kRowCount = 12;
		ui->tableWidget_modelInfo->setRowCount(kRowCount);
		ui->tableWidget_modelInfo->setColumnCount(2);
		ui->tableWidget_modelInfo->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		ui->tableWidget_modelInfo->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
		ui->tableWidget_modelInfo->verticalHeader()->setVisible(true);

		auto setRow = [&](int r, const QString& label, const QString& value,
			const QColor& valueColor = QColor())
		{
			auto* keyItem = new QTableWidgetItem(label);
			keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
			auto* valItem = new QTableWidgetItem(value);
			valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
			if (valueColor.isValid())
				valItem->setForeground(QBrush(valueColor));
			ui->tableWidget_modelInfo->setItem(r, 0, keyItem);
			ui->tableWidget_modelInfo->setItem(r, 1, valItem);
		};

		constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
		int r = 0;
		setRow(r++, QStringLiteral("名称"),     QString::fromStdString(info.base_info.name));
		// 套版关联：名称(编号) 蓝 / 未使用 灰 / 悬空引用（套版已删）橙
		if (currentStampPatternId_.empty())
		{
			setRow(r++, QStringLiteral("套版"), QStringLiteral("未使用"), QColor(141, 141, 141));
			setRow(r++, QStringLiteral("套版参数"), QStringLiteral("-"));
		}
		else if (currentStampValid_)
		{
			setRow(r++, QStringLiteral("套版"),
				QStringLiteral("%1 (%2)")
					.arg(QString::fromStdString(stampItem.info.base_info.name))
					.arg(QString::fromStdString(currentStampPatternId_)),
				QColor(QStringLiteral("#2196F3")));
			setRow(r++, QStringLiteral("套版参数"),
				QStringLiteral("透明度 %1，平移 %2/%3，旋转 %4°，缩放 %5%6")
					.arg(stampItem.data.alpha)
					.arg(stampItem.data.alignRow, 0, 'f', 1)
					.arg(stampItem.data.alignCol, 0, 'f', 1)
					.arg(stampItem.data.alignAngle * kRadToDeg, 0, 'f', 2)
					.arg(stampItem.data.alignScale, 0, 'f', 3)
					.arg(stampItem.data.fromDxf ? QStringLiteral(" (DXF)") : QString()));
		}
		else
		{
			setRow(r++, QStringLiteral("套版"),
				QStringLiteral("已失效: %1").arg(QString::fromStdString(currentStampPatternId_)),
				QColor(QStringLiteral("#E65100")));
			setRow(r++, QStringLiteral("套版参数"), QStringLiteral("-"));
		}
		setRow(r++, QStringLiteral("图像通道"), channelName(data._SingleChannelType));
		setRow(r++, QStringLiteral("预处理"),   preprocessSummary(data));
		setRow(r++, QStringLiteral("对比度"),   contrastSummary(data));
		setRow(r++, QStringLiteral("ROI / 屏蔽"),
			QStringLiteral("%1 / %2")
				.arg(static_cast<int>(data._paintCreateRoiList.size()))
				.arg(static_cast<int>(data._paintShieldRoiList.size())));
		setRow(r++, QStringLiteral("曝光1 / 增益1"),
			QStringLiteral("%1 / %2")
				.arg(data._createModelExposureTime, 0, 'f', 0)
				.arg(data._createModelGain, 0, 'f', 0));
		setRow(r++, QStringLiteral("曝光2 / 增益2"),
			QStringLiteral("%1 / %2")
				.arg(data._createModelExposureTime2, 0, 'f', 0)
				.arg(data._createModelGain2, 0, 'f', 0));
		setRow(r++, QStringLiteral("创建时间"), QString::fromStdString(info.getCreateTime()));
		setRow(r++, QStringLiteral("更新时间"), QString::fromStdString(info.getUpdateTime()));
		setRow(r++, QStringLiteral("文件夹"),   QString::fromStdString(info.getFolderPath()));

		// 双图预览：原图 / 模板图（支持拖拽缩放）
		if (data._originalImage.IsInitialized())
		{
			try { labelImgOriginal_->displayImage(data._originalImage); }
			catch (...) {}
		}

		if (data._templateMatImage.IsInitialized())
		{
			try { labelImgTemplate_->displayImage(data._templateMatImage); }
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
		fullKeyboard_->setValue(QString());
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

		const QString currentName = QString::fromStdString(
			allModels_.at(index.row()).base_info.name);
		fullKeyboard_->setValue(currentName);

		const int ret = fullKeyboard_->exec();
		if (ret != QDialog::Accepted)
			return;

		const QString newName = fullKeyboard_->getValue();
		if (newName.isEmpty() || newName == currentName)
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
		// 自动分配曝光（检测冲突，必要时弹窗）
		autoApplyExposure(ids, failedIds);
	}

	void ModelManagerDialog::onUnloadAll()
	{
		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		bun->unloadAllModels();
		refreshLoadedState();
	}

	void ModelManagerDialog::onPreviewStamp()
	{
		if (!currentStampValid_ || currentStampPatternId_.empty())
			return;

		auto& stampBun = app_.business().stamp_pattern_bun;
		if (!stampBun)
			return;

		const Config::StampPatternItem item = stampBun->getPatternItem(currentStampPatternId_);
		if (!item.data._patternImage.IsInitialized())
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("提示"), QStringLiteral("该套版缺少有效图片，无法预览。"));
			return;
		}

		// 套版按对齐参数叠加到模型原图（参考位姿，即生产匹配命中位姿处的显示效果）；
		// 无原图时退化为套版原始图案（RGBA -> RGB，4 通道直接显示仅 R 通道）
		HalconCpp::HImage preview;
		try
		{
			if (currentOriginalImage_.IsInitialized())
			{
				Config::StampPatternData d = item.data;
				// DXF 套版：图素单位为实际尺寸(mm)，需乘九点标定的像素/mm
				if (d.fromDxf)
					d.alignScale *= bun::StampPatternBun::pixelsPerWorldUnit(
						app_.business().infrastructure());
				const HalconCpp::HTuple H = bun::StampPatternBun::buildAlignHomMat2D(d);
				preview = bun::StampPatternBun::compositeOverlay(
					currentOriginalImage_, item.data._patternImage, H, item.data.alpha);
			}
			else
			{
				preview = item.data._patternImage;
				if (preview.CountChannels().I() == 4)
				{
					HalconCpp::HImage cr, cg, cb, ca;
					HalconCpp::Decompose4(preview, &cr, &cg, &cb, &ca);
					HalconCpp::Compose3(cr, cg, cb, &preview);
				}
			}
		}
		catch (...)
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("提示"), QStringLiteral("套版预览生成失败。"));
			return;
		}

		const QString stampName = QString::fromStdString(item.info.base_info.name);
		constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

		QDialog dlg(this);
		dlg.setWindowTitle(QStringLiteral("套版预览: %1").arg(stampName));
		auto* layout = new QVBoxLayout(&dlg);

		auto* imgLabel = new HalconInteractiveLabel(&dlg);
		imgLabel->setMinimumSize(640, 480);
		imgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		imgLabel->setAlignment(Qt::AlignCenter);
		layout->addWidget(imgLabel, 1);

		auto* infoLabel = new QLabel(
			QStringLiteral("编号: %1    透明度: %2    平移(行/列): %3 / %4    旋转: %5°    缩放: %6%7")
				.arg(QString::fromStdString(item.info.getId()))
				.arg(item.data.alpha)
				.arg(item.data.alignRow, 0, 'f', 1)
				.arg(item.data.alignCol, 0, 'f', 1)
				.arg(item.data.alignAngle * kRadToDeg, 0, 'f', 2)
				.arg(item.data.alignScale, 0, 'f', 3)
				.arg(item.data.fromDxf ? QStringLiteral(" (DXF)") : QString()),
			&dlg);
		infoLabel->setStyleSheet("font-size: 15px; color: #555; padding: 6px 0;");
		layout->addWidget(infoLabel);

		auto* btnLayout = new QHBoxLayout();
		btnLayout->addStretch();
		auto* btnClose = new QPushButton(QStringLiteral("关闭"), &dlg);
		btnClose->setStyleSheet("QPushButton { font-size: 16px; padding: 8px 24px; }");
		btnClose->setMinimumHeight(44);
		btnLayout->addWidget(btnClose);
		layout->addLayout(btnLayout);
		QObject::connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

		if (preview.IsInitialized())
		{
			try { imgLabel->displayImage(preview); }
			catch (...) {}
		}

		dlg.resize(960, 720);
		dlg.exec();
	}

	void ModelManagerDialog::autoApplyExposure(const std::vector<std::string>& loadedIds,
		const std::vector<std::string>& failedIds)
	{
		auto& bun = app_.business().shape_mode_manager_bun;
		if (!bun)
			return;

		// 过滤出加载成功的模型 ID
		std::set<std::string> failedSet(failedIds.begin(), failedIds.end());
		std::vector<std::string> successIds;
		for (const auto& id : loadedIds)
		{
			if (failedSet.find(id) == failedSet.end())
				successIds.push_back(id);
		}
		if (successIds.empty())
			return;

		// 收集每个成功加载模型的 (名称, 曝光1, 增益1, 曝光2, 增益2)
		struct ModelCamInfo
		{
			std::string id;
			QString name;
			double exposure1;
			double gain1;
			double exposure2;
			double gain2;
		};
		std::vector<ModelCamInfo> infos;

		auto& smmRef = app_.business().infrastructure().shape_model_manager_module_;
		if (!smmRef)
			return;
		auto* smm = smmRef.get();

		for (const auto& id : successIds)
		{
			try
			{
				auto item = smm->getShapeModelItem(id);
				ModelCamInfo info;
				info.id = id;
				info.name = QString::fromStdString(item.info.base_info.name);
				info.exposure1 = item.data._createModelExposureTime;
				info.gain1 = item.data._createModelGain;
				info.exposure2 = item.data._createModelExposureTime2;
				info.gain2 = item.data._createModelGain2;
				infos.push_back(info);
			}
			catch (...) {}
		}
		if (infos.empty())
			return;

		// 检查是否所有模型的曝光/增益一致（四路参数全部相同）
		const double firstExp1 = infos.front().exposure1;
		const double firstGain1 = infos.front().gain1;
		const double firstExp2 = infos.front().exposure2;
		const double firstGain2 = infos.front().gain2;
		const bool allSame = std::all_of(infos.begin(), infos.end(),
			[&](const ModelCamInfo& info) {
				return std::abs(info.exposure1 - firstExp1) < 0.5 &&
					std::abs(info.gain1 - firstGain1) < 0.5 &&
					std::abs(info.exposure2 - firstExp2) < 0.5 &&
					std::abs(info.gain2 - firstGain2) < 0.5;
			});

		if (allSame)
		{
			// 全部一致 → 静默应用
			bun->applyModelCameraSettings(infos.front().id);
			return;
		}

		// 存在冲突 → 弹窗让用户选择
		QDialog dlg(this);
		dlg.setWindowTitle(QStringLiteral("相机参数冲突"));
		dlg.setMinimumWidth(480);
		auto* layout = new QVBoxLayout(&dlg);

		auto* hint = new QLabel(QStringLiteral(
			"检测到多个模型的曝光/增益设置不一致，\n"
			"请选择应用哪个模型的相机参数："));
		hint->setStyleSheet("font-size: 15px; color: #E65100; padding: 8px 0;");
		layout->addWidget(hint);

		auto* group = new QButtonGroup(&dlg);
		auto* groupLayout = new QVBoxLayout();
		for (size_t i = 0; i < infos.size(); ++i)
		{
			const auto& info = infos[i];
			auto* radio = new QRadioButton(
				QStringLiteral("%1  (曝光: %2/%5 μs, 增益: %3/%6)")
					.arg(info.name)
					.arg(info.exposure1, 0, 'f', 0)
					.arg(info.gain1, 0, 'f', 0)
					.arg(info.exposure2, 0, 'f', 0)
					.arg(info.gain2, 0, 'f', 0),
				&dlg);
			radio->setStyleSheet("font-size: 16px; padding: 6px 0;");
			if (i == 0)
				radio->setChecked(true);
			group->addButton(radio, static_cast<int>(i));
			groupLayout->addWidget(radio);
		}
		layout->addLayout(groupLayout);

		auto* btnLayout = new QHBoxLayout();
		btnLayout->addStretch();
		auto* btnApply = new QPushButton(QStringLiteral("应用"), &dlg);
		btnApply->setStyleSheet(
			"QPushButton { font-size: 16px; padding: 8px 24px; font-weight: bold; }");
		auto* btnSkip = new QPushButton(QStringLiteral("跳过"), &dlg);
		btnSkip->setStyleSheet(
			"QPushButton { font-size: 16px; padding: 8px 24px; }");
		btnLayout->addWidget(btnApply);
		btnLayout->addWidget(btnSkip);
		layout->addLayout(btnLayout);

		QObject::connect(btnApply, &QPushButton::clicked, &dlg, &QDialog::accept);
		QObject::connect(btnSkip, &QPushButton::clicked, &dlg, &QDialog::reject);

		if (dlg.exec() == QDialog::Accepted)
		{
			const int idx = group->checkedId();
			if (idx >= 0 && idx < static_cast<int>(infos.size()))
				bun->applyModelCameraSettings(infos[idx].id);
		}
		// 用户点"跳过"→ 不操作
	}

	void ModelManagerDialog::onRenameModel()
	{
		const auto ids = selectedModelIds();
		if (ids.empty())
			return;

		const int row = selectedRow();
		if (row < 0 || row >= allModels_.size())
			return;

		// 重命名仅对焦点项生效
		const std::string id = allModels_.at(row).getId();
		const QString currentName = QString::fromStdString(
			allModels_.at(row).base_info.name);
		fullKeyboard_->setValue(currentName);

		const int ret = fullKeyboard_->exec();
		if (ret != QDialog::Accepted)
			return;

		const QString newName = fullKeyboard_->getValue();
		if (newName.isEmpty() || newName == currentName)
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
		app_.clearMainViewMatchRegion();
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
		app_.clearMainViewMatchRegion();
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
