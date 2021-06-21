/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2016  <copyright holder> <reimer@cbs.mpg.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "guiprogressfeedback.hpp"
#include "common.hpp"
#include <QProgressBar>
#include <QHBoxLayout>
#include <QVariant>
#include <iostream>

void isis::qt5::QProgressBarWrapper::update()
{
	if(progressBar){
		const auto ratio=int(current* progressBar->maximum()/max);
		emit sigSetVal(ratio);
	} else
		LOG(Debug,error) << "Calling show on progressbar wrapper without a progressbar";
}
isis::qt5::QProgressBarWrapper::QProgressBarWrapper(QProgressBar *_progressBar):progressBar(_progressBar)
{
	QObject::connect(progressBar,&QObject::destroyed, this, &QProgressBarWrapper::onBarDeleted);
	QObject::connect(this,&QProgressBarWrapper::sigSetVal,progressBar,&QProgressBar::setValue);
}
void isis::qt5::QProgressBarWrapper::show(size_t _max, std::string _header)
{
	if(progressBar){
		progressBar->setValue(0);
		progressBar->setFormat(QString::fromStdString(_header)+" %p%");
		progressBar->setTextVisible(true);
		restart(_max);
	} else
		LOG(Debug,error) << "Calling show on progressbar wrapper without a progressbar";

}
size_t isis::qt5::QProgressBarWrapper::progress(const std::string &message, size_t step)
{
	current+=step;
	update();
	return current;
}
void isis::qt5::QProgressBarWrapper::close()
{
	max=0;
	current=0;
	progressBar->setValue(0);
	progressBar->setTextVisible(false);
}
size_t isis::qt5::QProgressBarWrapper::getMax()
{
	return max;
}
size_t isis::qt5::QProgressBarWrapper::extend(size_t by)
{
	max+=by;
	update();
	return max;
}
void isis::qt5::QProgressBarWrapper::restart(size_t new_max)
{
	current=0;
	max=new_max;
	update();
}
void isis::qt5::QProgressBarWrapper::onBarDeleted(QObject *p)
{
	assert(p== nullptr || progressBar==p);
	progressBar = nullptr;
	close();
}

isis::qt5::GUIProgressFeedback::GUIProgressFeedback(bool autohide, QWidget* parent):QGroupBox(parent),QProgressBarWrapper(new QProgressBar(this)),show_always(!autohide)
{
	if(!autohide)
		QWidget::show();
	setLayout(new QHBoxLayout());
	layout()->addWidget(progressBar);
}
void isis::qt5::GUIProgressFeedback::close()
{
	QGroupBox::setProperty("visible",true);
	QProgressBarWrapper::close();
}
void isis::qt5::GUIProgressFeedback::show(size_t max, std::string header)
{
	QGroupBox::setProperty("title",QString::fromStdString(header));
	QGroupBox::setProperty("visible",true);
	QProgressBarWrapper::show(max,header);//this will display the header inside the progressbar
	progressBar->setFormat("%p%");//so we remove it here
}
