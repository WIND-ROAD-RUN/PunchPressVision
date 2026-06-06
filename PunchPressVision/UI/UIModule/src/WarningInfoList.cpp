#include "WarningInfoList.h"
#include "ui_WarningInfoList.h"

WarningInfoList::WarningInfoList(QWidget* parent)
	: QDialog(parent), ui(new Ui::WarningInfoListClass), _model(new QStandardItemModel(this))
{
	ui->setupUi(this);

	build_ui();
	build_connect();
}

WarningInfoList::~WarningInfoList()
{
	delete ui;
}

void WarningInfoList::build_ui()
{
	ui->tableView->setModel(_model);

	_model->setColumnCount(2);
	_model->setHeaderData(0, Qt::Horizontal, "时间戳");
	_model->setHeaderData(1, Qt::Horizontal, "警告信息");

	ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->tableView->horizontalHeader()->setStretchLastSection(true);
	ui->tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive); // 可调整
	ui->tableView->horizontalHeader()->resizeSection(0, 200);
}

void WarningInfoList::build_connect()
{
	connect(ui->pbtn_clear, &QPushButton::clicked, this, &WarningInfoList::onClearWarnings);
	connect(ui->pbtn_exit, &QPushButton::clicked, this, &WarningInfoList::hide);
}

void WarningInfoList::setWarningHistory(const std::deque<rw::rqw::WarningInfo>& history)
{
	_model->removeRows(0, _model->rowCount());

	for (const auto& entry : history) {
		QList<QStandardItem*> rowItems;
		rowItems.append(new QStandardItem(entry.timestamp.toString("yyyy-MM-dd HH:mm:ss")));
		rowItems.append(new QStandardItem(entry.message));
		_model->appendRow(rowItems);
	}
}

void WarningInfoList::onClearWarnings()
{
	_model->removeRows(0, _model->rowCount());

	emit clearWarnings();
}