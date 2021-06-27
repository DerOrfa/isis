//
// Created by enrico on 23.06.21.
//

// You may need to build the project (run Qt uic code generator) to get "ui_QLogWidget.h" resolved

#include "qlogwidget.hpp"
#include "ui_qlogwidget.h"
#include "qlogstore.hpp"
#include <QStyle>
#include <QObjectCleanupHandler>
#include <isis/core/singletons.hpp>


isis::qt5::QLogWidget::QLogWidget(QWidget *parent):QWidget(parent), ui(new Ui::QLogWidget)
{
	ui->setupUi(this);
	auto &log_store=isis::util::Singletons::get<isis::qt5::QLogStore, 10>();
	ui->log_view->setModel(&log_store);
}

isis::qt5::QLogWidget::~QLogWidget()
{
	delete ui;
}
bool isis::qt5::QLogWidget::registerButton(QAbstractButton *btn, bool use_as_feedback)
{
	if(use_as_feedback)
		feedback_button=btn;
	return feedback_connection=QObject::connect(btn,&QAbstractButton::clicked,this,&QWidget::show);
}
void isis::qt5::QLogWidget::onLogEvent(int level)
{
	if(feedback_connection)//check if connection is still there (hence button most likely still there)
	{
		switch(level)
		{
			case error:
				feedback_button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
			case warning:
				feedback_button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
			case notice:
			case info:
			case verbose_info:
				feedback_button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
		}
	}
}
void isis::qt5::QLogWidget::onShowError(bool toggled)  {showLevel(error,toggled);}
void isis::qt5::QLogWidget::onShowWarning(bool toggled){showLevel(warning,toggled);}
void isis::qt5::QLogWidget::onShowNotice(bool toggled) {showLevel(notice,toggled);}
void isis::qt5::QLogWidget::onShowInfo(bool toggled)   {showLevel(info,toggled);}
void isis::qt5::QLogWidget::onShowVerbose(bool toggled){showLevel(verbose_info,toggled);}

void isis::qt5::QLogWidget::showLevel(LogLevel level, bool show)
{

}
void isis::qt5::QLogWidget::hideLevel(LogLevel level){showLevel(level,false);}
