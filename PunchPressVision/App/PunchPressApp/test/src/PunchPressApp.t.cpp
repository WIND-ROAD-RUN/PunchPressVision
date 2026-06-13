#include "PunchPressApp.t.hpp"

#include "app/PunchPressApp.hpp"

#include <QCoreApplication>

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	inf::infrastructure inf;
	inf.build();
	bun::Business business(inf);
	business.build();
	business.start();
	app::PunchPressApp punchPressApp(business);
	punchPressApp.build();
	punchPressApp.start();


	return app.exec();
}
