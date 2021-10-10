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
#include <QStatusBar>
#include <QLabel>

isis::qt5::QProgressBarWrapper::QProgressBarWrapper(int display_max):disp_max(display_max){}

void isis::qt5::QProgressBarWrapper::update()
{
	if(max==0)
		return;
	const auto ratio=int(current* disp_max/max);
	emit sigSetVal(ratio);
}
void isis::qt5::QProgressBarWrapper::show(size_t _max, std::string _header)
{
	emit sigShow(QString::fromStdString(_header));
	emit sigSetVal(0);
	restart(_max);
}
size_t isis::qt5::QProgressBarWrapper::progress(size_t step)
{
	current+=step;
	update();
	return current;
}
void isis::qt5::QProgressBarWrapper::close()
{
	max=0;
	current=0;
	emit sigClose();
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

isis::qt5::QStatusBarProgress::QStatusBarProgress(QStatusBar *_status_bar)
:status_bar(_status_bar),progress_bar(new QProgressBar),header_bar(new QLabel)
{
	assert(status_bar);
	connect(this,&QProgressBarWrapper::sigShow,this,&QStatusBarProgress::onShow);
	connect(this,&QProgressBarWrapper::sigClose,this,&QStatusBarProgress::onClose);
	connect(this,&QProgressBarWrapper::sigSetVal,progress_bar,&QProgressBar::setValue);
}
void isis::qt5::QStatusBarProgress::onShow(QString header)
{
	if(status_bar){
		header_bar->setText(header);
		progress_bar->setValue(0);
		status_bar->insertWidget(0,header_bar);
		status_bar->insertWidget(1,progress_bar);
		header_bar->show();
		progress_bar->show();
	} else
		LOG(Debug,error) << "Calling show on status bar that was deleted";
}
void isis::qt5::QStatusBarProgress::onClose()
{
	if(status_bar){
		header_bar->clear();
		status_bar->removeWidget(header_bar);
		progress_bar->setValue(0);
		status_bar->removeWidget(progress_bar);
	} else
		LOG(Debug,error) << "Calling close on status bar that was deleted";
}
