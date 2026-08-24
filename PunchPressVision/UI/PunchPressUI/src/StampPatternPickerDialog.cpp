// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>

#include "UI/StampPatternPickerDialog.h"
#include "ui_DlgStampPatternPicker.h"

#include <QShowEvent>
#include <QListView>

#include "app/PunchPressApp.hpp"
#include "Business/StampPatternBun/StampPatternBun.hpp"

#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	StampPatternPickerDialog::StampPatternPickerDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::DlgStampPatternPickerClass())
		, app_(app)
		, listModel_(new StampPatternListModel(this))
	{
		ui->setupUi(this);

#ifdef PPV_RELEASE_FULLSCREEN
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setWindowState(windowState() | Qt::WindowFullScreen);
#endif

		ui->listView_patternList->setSelectionMode(QAbstractItemView::SingleSelection);
		ui->listView_patternList->setModel(listModel_);

		// 预览区域：替换 QLabel 占位为 HalconInteractiveLabel（支持拖拽缩放）
		ui->vLayout_preview->removeWidget(ui->label_imgPreview);
		ui->label_imgPreview->hide();
		labelImgPreview_ = new HalconInteractiveLabel(this);
		labelImgPreview_->setMinimumSize(200, 150);
		labelImgPreview_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		labelImgPreview_->setAlignment(Qt::AlignCenter);
		ui->vLayout_preview->addWidget(labelImgPreview_);

		buildConnections();
	}

	StampPatternPickerDialog::~StampPatternPickerDialog()
	{
		delete ui;
	}

	void StampPatternPickerDialog::buildConnections()
	{
		connect(ui->listView_patternList->selectionModel(), &QItemSelectionModel::currentChanged,
			this, &StampPatternPickerDialog::onListSelectionChanged);
		connect(ui->listView_patternList, &QListView::doubleClicked,
			this, &StampPatternPickerDialog::onListDoubleClicked);

		connect(ui->pbtn_use, &QPushButton::clicked,
			this, &StampPatternPickerDialog::onUse);
		connect(ui->pbtn_useNone, &QPushButton::clicked,
			this, &StampPatternPickerDialog::onUseNone);
		connect(ui->pbtn_cancel, &QPushButton::clicked,
			this, &StampPatternPickerDialog::onCancel);
	}

	void StampPatternPickerDialog::showEvent(QShowEvent* event)
	{
		QDialog::showEvent(event);
		if (parentWidget())
			resize(parentWidget()->size());

		refreshPatternList();

		// 预选中当前已关联的套版；未设置或已不存在时选中第一项
		int preselect = 0;
		if (!initialId_.empty())
		{
			const QString qid = QString::fromStdString(initialId_);
			for (int i = 0; i < allPatterns_.size(); ++i)
			{
				if (QString::fromStdString(allPatterns_.at(i).getId()) == qid)
				{
					preselect = i;
					break;
				}
			}
		}
		if (listModel_->patternCount() > 0)
		{
			ui->listView_patternList->setCurrentIndex(listModel_->index(preselect, 0));
			refreshPreview(preselect);
		}
	}

	// ===== 数据刷新 =====

	void StampPatternPickerDialog::refreshPatternList()
	{
		auto& bun = app_.business().stamp_pattern_bun;
		if (!bun)
		{
			allPatterns_.clear();
			listModel_->setPatternInfos(allPatterns_);
			return;
		}

		const auto raw = bun->getAllPatterns();
		allPatterns_ = QVector<Config::StampPatternInfo>(raw.begin(), raw.end());
		listModel_->setPatternInfos(allPatterns_);
	}

	void StampPatternPickerDialog::refreshPreview(int row)
	{
		if (!labelImgPreview_)
			return;

		if (row < 0 || row >= allPatterns_.size())
		{
			labelImgPreview_->clear();
			return;
		}

		auto& bun = app_.business().stamp_pattern_bun;
		if (!bun)
		{
			labelImgPreview_->clear();
			return;
		}

		const Config::StampPatternItem item = bun->getPatternItem(allPatterns_.at(row).getId());
		if (item.data._patternImage.IsInitialized())
		{
			// 4 通道 RGBA 在 Halcon 窗口中仅显示第一通道（红十字所在 R 通道），
			// 合成 RGB 三通道预览才能看到蓝色线条
			try
			{
				HalconCpp::HImage preview = item.data._patternImage;
				if (preview.CountChannels().I() == 4)
				{
					HalconCpp::HImage r, g, b, a;
					HalconCpp::Decompose4(preview, &r, &g, &b, &a);
					HalconCpp::Compose3(r, g, b, &preview);
				}
				labelImgPreview_->displayImage(preview);
			}
			catch (...) {}
		}
		else
		{
			labelImgPreview_->clear();
		}
	}

	int StampPatternPickerDialog::selectedRow() const
	{
		const auto idx = ui->listView_patternList->currentIndex();
		return idx.isValid() ? idx.row() : -1;
	}

	void StampPatternPickerDialog::acceptWithId(const std::string& id)
	{
		selectedId_ = id;
		accept();
	}

	// ===== 交互 =====

	void StampPatternPickerDialog::onListSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
	{
		if (!current.isValid())
			return;
		refreshPreview(current.row());
	}

	void StampPatternPickerDialog::onListDoubleClicked(const QModelIndex& index)
	{
		if (!index.isValid() || index.row() < 0 || index.row() >= allPatterns_.size())
			return;
		acceptWithId(allPatterns_.at(index.row()).getId());
	}

	void StampPatternPickerDialog::onUse()
	{
		const int row = selectedRow();
		if (row < 0)
		{
			rw::rqwu::MessageBox::information(this,
				QStringLiteral("提示"), QStringLiteral("请先在列表中选择一个套版"));
			return;
		}
		acceptWithId(allPatterns_.at(row).getId());
	}

	void StampPatternPickerDialog::onUseNone()
	{
		acceptWithId({});
	}

	void StampPatternPickerDialog::onCancel()
	{
		reject();
	}
}
