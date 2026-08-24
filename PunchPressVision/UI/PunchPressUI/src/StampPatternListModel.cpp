#include "UI/StampPatternListModel.h"

namespace ui
{
	StampPatternListModel::StampPatternListModel(QObject* parent)
		: QAbstractListModel(parent)
	{
	}

	int StampPatternListModel::rowCount(const QModelIndex& parent) const
	{
		if (parent.isValid())
			return 0;
		return static_cast<int>(infos_.size());
	}

	QVariant StampPatternListModel::data(const QModelIndex& index, int role) const
	{
		if (!index.isValid() || index.row() < 0 || index.row() >= infos_.size())
			return {};

		const auto& info = infos_.at(index.row());

		switch (role)
		{
		case Qt::DisplayRole:
			return QString::fromStdString(info.base_info.name);
		case PatternIdRole:
			return QString::fromStdString(info.getId());
		default:
			return {};
		}
	}

	void StampPatternListModel::setPatternInfos(const QVector<Config::StampPatternInfo>& infos)
	{
		beginResetModel();
		infos_ = infos;
		endResetModel();
	}

	const Config::StampPatternInfo& StampPatternListModel::patternInfoAt(int row) const
	{
		return infos_.at(row);
	}

	int StampPatternListModel::patternCount() const
	{
		return static_cast<int>(infos_.size());
	}
}
