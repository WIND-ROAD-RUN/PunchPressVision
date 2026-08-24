// 必须最先包含：在 windows.h 定义 MessageBox 宏之前解析 rqwu 头。
#include <rwul/rqwu/rqwu_MessageBox.h>
#include <rwul/rqwu/Keyboard/rqwu_FullKeyboard.h>

#include "UI/StampPatternManagerDialog.h"
#include "ui_DlgStampPatternManager.h"

#include <QShowEvent>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QUuid>

#include "app/PunchPressApp.hpp"
#include "Business/StampPatternBun/StampPatternBun.hpp"
#include "UI/DxfRegionSelectDialog.h"

#ifdef MessageBox
#undef MessageBox
#endif

namespace ui
{
	StampPatternManagerDialog::StampPatternManagerDialog(app::PunchPressApp& app, QWidget* parent)
		: QDialog(parent)
		, ui(new Ui::DlgStampPatternManagerClass())
		, app_(app)
		, listModel_(new StampPatternListModel(this))
	{
		ui->setupUi(this);

#ifdef PPV_RELEASE_FULLSCREEN
		setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
		setWindowState(windowState() | Qt::WindowFullScreen);
#endif

		fullKeyboard_ = new rw::rqwu::FullKeyboard(this);
		fullKeyboard_->emptyInputPolicy = rw::rqwu::Keyboard::EmptyInputPolicy::EnableAndAccept;

		ui->listView_patternList->setSelectionMode(QAbstractItemView::SingleSelection);
		ui->listView_patternList->setModel(listModel_);

		// 详情表格
		ui->tableWidget_patternInfo->horizontalHeader()->setVisible(false);
		ui->tableWidget_patternInfo->verticalHeader()->setVisible(false);
		ui->tableWidget_patternInfo->setEditTriggers(QAbstractItemView::NoEditTriggers);
		ui->tableWidget_patternInfo->setSelectionMode(QAbstractItemView::NoSelection);
		ui->tableWidget_patternInfo->setFocusPolicy(Qt::NoFocus);

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

	StampPatternManagerDialog::~StampPatternManagerDialog()
	{
		delete ui;
	}

	void StampPatternManagerDialog::buildConnections()
	{
		connect(ui->listView_patternList->selectionModel(), &QItemSelectionModel::currentChanged,
			this, &StampPatternManagerDialog::onListSelectionChanged);

		connect(ui->pbtn_import, &QPushButton::clicked,
			this, &StampPatternManagerDialog::onImport);
		connect(ui->pbtn_rename, &QPushButton::clicked,
			this, &StampPatternManagerDialog::onRename);
		connect(ui->pbtn_delete, &QPushButton::clicked,
			this, &StampPatternManagerDialog::onDelete);
		connect(ui->pbtn_exit, &QPushButton::clicked,
			this, &StampPatternManagerDialog::onExit);
	}

	void StampPatternManagerDialog::showEvent(QShowEvent* event)
	{
		QDialog::showEvent(event);
		if (parentWidget())
			resize(parentWidget()->size());

		refreshPatternList();

		if (listModel_->patternCount() > 0)
		{
			ui->listView_patternList->setCurrentIndex(listModel_->index(0, 0));
			refreshPatternDetail(0);
		}
	}

	// ===== 数据刷新 =====

	void StampPatternManagerDialog::refreshPatternList()
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

	void StampPatternManagerDialog::refreshPatternDetail(int row)
	{
		if (row < 0 || row >= allPatterns_.size())
		{
			ui->tableWidget_patternInfo->setRowCount(0);
			if (labelImgPreview_)
				labelImgPreview_->clear();
			return;
		}

		const auto& info = allPatterns_.at(row);

		// 从图库读取图片 + 对齐参数
		Config::StampPatternItem item;
		auto& bun = app_.business().stamp_pattern_bun;
		if (bun)
			item = bun->getPatternItem(info.getId());

		constexpr int kRowCount = 7;
		ui->tableWidget_patternInfo->setRowCount(kRowCount);
		ui->tableWidget_patternInfo->setColumnCount(2);
		ui->tableWidget_patternInfo->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		ui->tableWidget_patternInfo->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

		auto setRow = [&](int r, const QString& label, const QString& value)
		{
			auto* keyItem = new QTableWidgetItem(label);
			keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
			auto* valItem = new QTableWidgetItem(value);
			valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
			ui->tableWidget_patternInfo->setItem(r, 0, keyItem);
			ui->tableWidget_patternInfo->setItem(r, 1, valItem);
		};

		constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
		int r = 0;
		setRow(r++, QStringLiteral("名称"),     QString::fromStdString(info.base_info.name));
		setRow(r++, QStringLiteral("创建时间"), QString::fromStdString(info.getCreateTime()));
		setRow(r++, QStringLiteral("更新时间"), QString::fromStdString(info.getUpdateTime()));
		setRow(r++, QStringLiteral("透明度"),   QString::number(item.data.alpha));
		setRow(r++, QStringLiteral("对齐平移(行/列)"),
			QStringLiteral("%1 / %2").arg(item.data.alignRow, 0, 'f', 1).arg(item.data.alignCol, 0, 'f', 1));
		setRow(r++, QStringLiteral("对齐旋转/缩放"),
			QStringLiteral("%1° / %2")
				.arg(item.data.alignAngle * kRadToDeg, 0, 'f', 2)
				.arg(item.data.alignScale, 0, 'f', 3));
		setRow(r++, QStringLiteral("文件夹"),   QString::fromStdString(info.getFolderPath()));

		// 预览图片
		if (labelImgPreview_)
		{
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
	}

	int StampPatternManagerDialog::selectedRow() const
	{
		const auto idx = ui->listView_patternList->currentIndex();
		return idx.isValid() ? idx.row() : -1;
	}

	// ===== 交互 =====

	void StampPatternManagerDialog::onListSelectionChanged(const QModelIndex& current, const QModelIndex& /*previous*/)
	{
		if (!current.isValid())
			return;
		refreshPatternDetail(current.row());
	}

	void StampPatternManagerDialog::onImport()
	{
		const QString filePath = QFileDialog::getOpenFileName(this,
			QStringLiteral("选择套版文件"),
			QString(),
			QStringLiteral("套版文件 (*.dxf *.png *.bmp *.jpg *.jpeg *.tif *.tiff);;CAD 图纸 (*.dxf);;图片 (*.png *.bmp *.jpg *.jpeg *.tif *.tiff)"));
		if (filePath.isEmpty())
			return;

		// DXF 图纸：先弹区域选择对话框（多图案图纸框选所需部分），
		// 确定后将选中轮廓渲染为临时 RGBA PNG，再走统一图片导入
		std::string importPath = filePath.toStdString();
		QString tempPng;
		if (filePath.endsWith(QStringLiteral(".dxf"), Qt::CaseInsensitive))
		{
			DxfRegionSelectDialog dlg(filePath, this);
			if (dlg.exec() != QDialog::Accepted)
				return;

			tempPng = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
				.filePath(QStringLiteral("ppv_dxf_%1.png")
					.arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
			if (!inf::renderDxfContoursToRgbaPng(dlg.selectedContours(), tempPng.toStdString()))
			{
				rw::rqwu::MessageBox::warning(this,
					QStringLiteral("导入失败"),
					QStringLiteral("DXF 渲染失败，请检查图纸内容。"));
				QFile::remove(tempPng);
				return;
			}
			importPath = tempPng.toStdString();
		}

		// 默认名称 = 文件名（去扩展名）
		const QString defaultName = QFileInfo(filePath).completeBaseName();

		fullKeyboard_->setValue(defaultName);
		if (fullKeyboard_->exec() != QDialog::Accepted)
		{
			if (!tempPng.isEmpty())
				QFile::remove(tempPng);
			return;
		}

		QString name = fullKeyboard_->getValue().trimmed();
		if (name.isEmpty())
			name = defaultName;

		auto& bun = app_.business().stamp_pattern_bun;
		if (!bun)
		{
			if (!tempPng.isEmpty())
				QFile::remove(tempPng);
			return;
		}

		const Config::StampPatternInfo info = bun->importPattern(
			importPath, name.toStdString());

		if (!tempPng.isEmpty())
			QFile::remove(tempPng);

		if (info.getId().empty())
		{
			rw::rqwu::MessageBox::warning(this,
				QStringLiteral("导入失败"),
				QStringLiteral("无法导入所选文件，请检查文件是否为有效图片或 CAD DXF 图纸。"));
			return;
		}

		refreshPatternList();

		// 新导入的套版在最上方，选中并预览
		if (listModel_->patternCount() > 0)
		{
			ui->listView_patternList->setCurrentIndex(listModel_->index(0, 0));
			refreshPatternDetail(0);
		}
	}

	void StampPatternManagerDialog::onRename()
	{
		const int row = selectedRow();
		if (row < 0)
			return;

		const std::string id = allPatterns_.at(row).getId();
		const QString currentName = QString::fromStdString(allPatterns_.at(row).base_info.name);

		fullKeyboard_->setValue(currentName);
		if (fullKeyboard_->exec() != QDialog::Accepted)
			return;

		const QString newName = fullKeyboard_->getValue().trimmed();
		if (newName.isEmpty() || newName == currentName)
			return;

		auto& bun = app_.business().stamp_pattern_bun;
		if (!bun)
			return;

		bun->renamePattern(id, newName.toStdString());
		refreshPatternList();
	}

	void StampPatternManagerDialog::onDelete()
	{
		const int row = selectedRow();
		if (row < 0)
			return;

		const QString name = QString::fromStdString(allPatterns_.at(row).base_info.name);
		if (rw::rqwu::MessageBox::question(this,
			QStringLiteral("确认删除"),
			QStringLiteral("确定要删除套版 \"%1\" 吗？").arg(name))
			!= rw::rqwu::MessageBox::StandardButton::Yes)
			return;

		auto& bun = app_.business().stamp_pattern_bun;
		if (!bun)
			return;

		const std::string id = allPatterns_.at(row).getId();
		bun->deletePattern(id);

		refreshPatternList();

		if (listModel_->patternCount() > 0)
		{
			ui->listView_patternList->setCurrentIndex(listModel_->index(0, 0));
			refreshPatternDetail(0);
		}
	}

	void StampPatternManagerDialog::onExit()
	{
		close();
	}
}
