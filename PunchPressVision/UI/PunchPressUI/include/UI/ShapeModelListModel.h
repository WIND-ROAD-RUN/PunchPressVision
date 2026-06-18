#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QFont>
#include <QSet>
#include <QVector>
#include <set>

#include "infrastructure/ShapeModelManagerModule/Config/ShapeModelItem.hpp"

namespace ui
{
	/// <summary>
	/// 用于 QListView 的模型列表数据模型。
	/// 包装 Config::ShapeModelInfo 向量，提供 Qt Model-View 接口。
	/// 支持标记已加载状态，供多模型场景使用。
	/// </summary>
	class ShapeModelListModel : public QAbstractListModel
	{
		Q_OBJECT

	public:
		/// 自定义角色
		enum ShapeModelRoles
		{
			ModelIdRole = Qt::UserRole,  // 模型 ID (QString)
			IsLoadedRole,                // 是否已加载 (bool)
		};

		explicit ShapeModelListModel(QObject* parent = nullptr);

		int rowCount(const QModelIndex& parent = QModelIndex()) const override;
		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

		void setModelInfos(const QVector<Config::ShapeModelInfo>& infos);
		/// <summary>设置当前已加载的模型 ID 集合，触发视图刷新。</summary>
		void setLoadedModelIds(const std::set<std::string>& ids);
		const Config::ShapeModelInfo& modelInfoAt(int row) const;
		int modelCount() const;

	private:
		bool isModelLoaded(int row) const;

		QVector<Config::ShapeModelInfo> infos_;
		QSet<QString> loadedIds_;  // 已加载模型 ID 集合
	};
}
