#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "infrastructure/StampPatternModule/Config/StampPatternItem.hpp"

namespace ui
{
	/// <summary>
	/// 套版图库列表模型。包装 Config::StampPatternInfo 向量，提供 Qt Model-View 接口。
	/// </summary>
	class StampPatternListModel : public QAbstractListModel
	{
		Q_OBJECT

	public:
		enum StampPatternRoles
		{
			PatternIdRole = Qt::UserRole,  // 套版 ID (QString)
		};

		explicit StampPatternListModel(QObject* parent = nullptr);

		int rowCount(const QModelIndex& parent = QModelIndex()) const override;
		QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

		void setPatternInfos(const QVector<Config::StampPatternInfo>& infos);
		const Config::StampPatternInfo& patternInfoAt(int row) const;
		int patternCount() const;

	private:
		QVector<Config::StampPatternInfo> infos_;
	};
}
