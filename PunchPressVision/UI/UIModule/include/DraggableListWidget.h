#pragma once
#include <QListWidget>
#include <QMouseEvent>
#include <QScrollBar>

class DraggableListWidget : public QListWidget
{
	Q_OBJECT
public:
	explicit DraggableListWidget(QWidget* parent = nullptr)
		: QListWidget(parent) {
	}

protected:
	void mousePressEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			m_dragging = true;
			m_lastMousePos = event->pos();
			setCursor(Qt::ClosedHandCursor);
		}
		QListWidget::mousePressEvent(event);
	}

	void mouseMoveEvent(QMouseEvent* event) override {
		if (m_dragging) {
			int dy = event->pos().y() - m_lastMousePos.y();
			verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
			m_lastMousePos = event->pos();
		}
		QListWidget::mouseMoveEvent(event);
	}

	void mouseReleaseEvent(QMouseEvent* event) override {
		if (event->button() == Qt::LeftButton) {
			m_dragging = false;
			unsetCursor();
		}
		QListWidget::mouseReleaseEvent(event);
	}

private:
	bool m_dragging = false;
	QPoint m_lastMousePos;
};