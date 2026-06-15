#include "UI/ShapeModelListModel.h"

namespace ui
{
	ShapeModelListModel::ShapeModelListModel(QObject* parent)
		: QAbstractListModel(parent)
	{
	}

	int ShapeModelListModel::rowCount(const QModelIndex& parent) const
	{
		if (parent.isValid())
			return 0;
		return static_cast<int>(infos_.size());
	}

	QVariant ShapeModelListModel::data(const QModelIndex& index, int role) const
	{
		if (!index.isValid() || index.row() < 0 || index.row() >= infos_.size())
			return {};

		if (role == Qt::DisplayRole)
			return QString::fromStdString(infos_.at(index.row()).base_info.name);

		if (role == Qt::UserRole)
			return QString::fromStdString(infos_.at(index.row()).getId());

		return {};
	}

	void ShapeModelListModel::setModelInfos(const QVector<Config::ShapeModelInfo>& infos)
	{
		beginResetModel();
		infos_ = infos;
		endResetModel();
	}

	const Config::ShapeModelInfo& ShapeModelListModel::modelInfoAt(int row) const
	{
		return infos_.at(row);
	}

	int ShapeModelListModel::modelCount() const
	{
		return static_cast<int>(infos_.size());
	}
}
