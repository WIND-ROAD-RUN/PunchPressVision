#pragma once

#include <QLabel>
#include <QWidget>
#include <QMouseEvent>

namespace rw
{
	namespace rqw
	{
		class ClickableLabel : public QLabel
		{
			Q_OBJECT

		public:
			explicit ClickableLabel(QWidget* parent = nullptr) : QLabel(parent) {}
			~ClickableLabel() {}

		signals:
			void clicked(); // 定义一个信号

		protected:
			void mousePressEvent(QMouseEvent* event) override;
		};
	}
}
