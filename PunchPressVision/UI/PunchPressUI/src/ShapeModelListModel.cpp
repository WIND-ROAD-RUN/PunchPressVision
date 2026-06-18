#include "UI/ShapeModelListModel.h"

#include <algorithm>

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

		const auto& info = infos_.at(index.row());
		const bool loaded = isModelLoaded(index.row());

		switch (role)
		{
		case Qt::DisplayRole:
		{
			const QString name = QString::fromStdString(info.base_info.name);
			// 已加载模型前加实心圆标记
			return loaded ? QStringLiteral("● ") + name : name;
		}
		case Qt::UserRole:  // ModelIdRole — 兼容旧代码
			return QString::fromStdString(info.getId());
		case IsLoadedRole:
			return loaded;
		case Qt::ForegroundRole:
			// 已加载模型使用蓝色高亮
			return loaded ? QColor(0x2196F3) : QVariant{};
		case Qt::FontRole:
			if (loaded)
			{
				QFont font;
				font.setBold(true);
				return font;
			}
			return QVariant{};
		default:
			return {};
		}
	}

	void ShapeModelListModel::setModelInfos(const QVector<Config::ShapeModelInfo>& infos)
	{
		beginResetModel();
		infos_ = infos;
		endResetModel();
	}

	void ShapeModelListModel::setLoadedModelIds(const std::set<std::string>& ids)
	{
		loadedIds_.clear();
		for (const auto& id : ids)
			loadedIds_.insert(QString::fromStdString(id));

		// 触发视图刷新（重绘所有项的颜色/字体/文本）
		if (!infos_.isEmpty())
			emit dataChanged(index(0), index(static_cast<int>(infos_.size()) - 1),
				{ Qt::DisplayRole, Qt::ForegroundRole, Qt::FontRole, IsLoadedRole });
	}

	const Config::ShapeModelInfo& ShapeModelListModel::modelInfoAt(int row) const
	{
		return infos_.at(row);
	}

	int ShapeModelListModel::modelCount() const
	{
		return static_cast<int>(infos_.size());
	}

	bool ShapeModelListModel::isModelLoaded(int row) const
	{
		if (row < 0 || row >= infos_.size())
			return false;
		return loadedIds_.contains(QString::fromStdString(infos_.at(row).getId()));
	}
}
