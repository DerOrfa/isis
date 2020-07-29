#include <iostream>
#include <isis/adapter/qt5/common.hpp>
#include <isis/adapter/qt5/qtapplication.hpp>
#include <isis/adapter/qt5/display.hpp>

using namespace isis::qt5;

int main(int argc, char **argv) {

	IOQtApplication app("isisview",true,false);
	app.init(argc,argv);

	while(!app.images.empty()){
		display(app.fetchImage());
	}
    return app.exec();
}

