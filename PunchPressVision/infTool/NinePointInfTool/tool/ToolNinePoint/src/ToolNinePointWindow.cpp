#include "ToolNinePointWindow.hpp"

ToolNinePointWindow::ToolNinePointWindow(QWidget* parent)
	: QMainWindow(parent)
{
	setWindowTitle(QStringLiteral("ToolNinePoint 九点标定工具"));
	resize(800, 600);
}

ToolNinePointWindow::~ToolNinePointWindow() = default;
