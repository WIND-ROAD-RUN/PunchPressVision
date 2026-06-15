#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

namespace ui
{
	/// <summary>
	/// 用于 QListView 的模型列表数据模型。
	/// 包装 Config::ShapeModelInfo 向量，提供 Qt Model-View 接口。
	/// </summary>
	class ShapeModelListModel : public QAbstractListModel
	{
		Q_OBJECT

	public:
		explicit ShapeModelListModel(QObject* parent = nullptr);

		int rowCount(const QModelIndex& parent = QModelIndex()) const override;
		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

		void setModelInfos(const QVector<Config::ShapeModelInfo>& infos);
		const Config::ShapeModelInfo& modelInfoAt(int row) const;
		int modelCount() const;

	private:
		QVector<Config::ShapeModelInfo> infos_;
	};
}
