#include "rqw_LabelClickable.h"

namespace rw
{
	namespace rqw
	{
		void ClickableLabel::mousePressEvent(QMouseEvent* event)
		{
			if (event->button() == Qt::LeftButton) {
				emit clicked(); // 触发信号
			}
			QLabel::mousePressEvent(event);
		}
	}
}