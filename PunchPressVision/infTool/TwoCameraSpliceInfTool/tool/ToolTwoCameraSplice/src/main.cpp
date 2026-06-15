#include "ToolTwoCameraSpliceWindow.hpp"

#include <QApplication>

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	ToolTwoCameraSpliceWindow wnd;
	wnd.show();

	return app.exec();
}
