//
// Created by enrico on 23.06.21.
//

// You may need to build the project (run Qt uic code generator) to get "ui_QLogWidget.h" resolved

#include "qlogwidget.hpp"
#include "ui_qlogwidget.h"
#include <QStyle>
#include <QObjectCleanupHandler>


isis::qt5::QLogWidget::QLogWidget(QWidget *parent):QWidget(parent), ui(new Ui::QLogWidget)
{
	ui->setupUi(this);
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
void isis::qt5::QLogWidget::onLogEvent(LogEvent event)
{
	if(feedback_connection)//check if connection is still there (hence button most likely still there)
	{
		switch(event.m_level)
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

