#include "ToolTwoCameraSpliceWindow.hpp"

ToolTwoCameraSpliceWindow::ToolTwoCameraSpliceWindow(QWidget* parent)
	: QMainWindow(parent)
{
	setWindowTitle(QStringLiteral("ToolTwoCameraSplice 双相机拼接工具"));
	resize(800, 600);
}

ToolTwoCameraSpliceWindow::~ToolTwoCameraSpliceWindow() = default;
