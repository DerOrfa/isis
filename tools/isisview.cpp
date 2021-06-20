#include <iostream>
#include <isis/adapter/qt5/qtapplication.hpp>
#include <isis/adapter/qt5/simpleimageview.hpp>

using namespace isis::qt5;

int main(int argc, char **argv) {

	IOQtApplication app("isisview",true,false);

	auto main = app.init(argc,argv,&MainImageView::images_loaded);
	main->setWindowTitle("ISISView");
	main->show();
    return app.exec();
}

