#include <iostream>
#include <isis/adapter/qt5/qtapplication.hpp>
#include <isis/adapter/qt5/simpleimageview.hpp>
#include <isis/adapter/qt5/qlogwidget.hpp>
#include <QPushButton>
#include <QStyle>
#include <QStatusBar>

using namespace isis::qt5;

int main(int argc, char **argv) {

	IOQtApplication app("isisview",true,false);

	auto main = app.init(argc,argv,&MainImageView::images_loaded);
	main->setWindowTitle("ISISView");

	auto log_widget=new QLogWidget;
	auto log_btn=new QPushButton(main->style()->standardIcon(QStyle::SP_MessageBoxQuestion),"");
	log_btn->setToolTip("Click for logging");
	main->statusBar()->addPermanentWidget(log_btn);
	log_widget->registerButton(log_btn,true);

	main->show();
    return app.exec();
}

