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
	proxy = new QSortFilterProxyModel(this);
	auto &log_store=isis::util::Singletons::get<isis::qt5::QLogStore, 10>();
	proxy->setSourceModel(&log_store);
	proxy->setFilterKeyColumn(1);//filter by severity (see updateSeverityFilter)
	ui->log_view->setModel(proxy);
	QObject::connect(&log_store,&QLogStore::notify,this,&QLogWidget::onLogEvent);
	QObject::connect(ui->show_error,&QAbstractButton::toggled,this,&QLogWidget::updateSeverityFilter);
	QObject::connect(ui->show_warning,&QAbstractButton::toggled,this,&QLogWidget::updateSeverityFilter);
	QObject::connect(ui->show_notice,&QAbstractButton::toggled,this,&QLogWidget::updateSeverityFilter);
	QObject::connect(ui->show_info,&QAbstractButton::toggled,this,&QLogWidget::updateSeverityFilter);
	QObject::connect(ui->show_verbose,&QAbstractButton::toggled,this,&QLogWidget::updateSeverityFilter);
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
				break;
			case warning:
				feedback_button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
				break;
			case notice:
			case info:
			case verbose_info:
				feedback_button->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
				break;
			default:
				assert(false);
		}
	}
}
void isis::qt5::QLogWidget::updateSeverityFilter()
{
	QStringList l_filter;
	if(ui->show_error->isChecked())
		l_filter << "error";
	if(ui->show_warning->isChecked())
		l_filter << "warning";
	if(ui->show_notice->isChecked())
		l_filter << "notice";
	if(ui->show_info->isChecked())
		l_filter << "info";
	if(ui->show_verbose->isChecked())
		l_filter << "verbose";
	proxy->setFilterRegExp(l_filter.join('|'));
}
