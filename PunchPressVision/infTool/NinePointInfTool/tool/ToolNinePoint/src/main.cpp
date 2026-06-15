#include "ToolNinePointWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	ToolNinePointWindow wnd;
	wnd.show();

	return app.exec();
}
