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
#include <QHBoxLayout>
#include <QVariant>
#include <iostream>

void isis::qt5::QProgressBarWrapper::update()
{
	if(max==0)
		return;
	if(progress_bar){
		const auto ratio=int(current* progress_bar->maximum()/max);
		emit sigSetVal(ratio);
	} else
		LOG(Debug,error) << "Calling show on progressbar wrapper without a progressbar";
}
isis::qt5::QProgressBarWrapper::QProgressBarWrapper(QProgressBar *_progressBar): progress_bar(_progressBar)
{
	assert(_progressBar);
	QObject::connect(progress_bar, &QObject::destroyed, this, &QProgressBarWrapper::onBarDeleted);
	QObject::connect(this, &QProgressBarWrapper::sigSetVal, progress_bar, &QProgressBar::setValue);
	progress_bar->setTextVisible(true);
}
void isis::qt5::QProgressBarWrapper::show(size_t _max, std::string _header)
{
	if(progress_bar){
		emit sigSetVal(0);
		restart(_max);
	} else
		LOG(Debug,error) << "Calling show on progressbar wrapper without a progressbar";

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
	emit sigSetVal(0);
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
	assert(p== nullptr || progress_bar==p);
	progress_bar = nullptr;
	close();
}

isis::qt5::QStatusBarProgress::QStatusBarProgress(QStatusBar *_status_bar): QProgressBarWrapper(new QProgressBar),status_bar(_status_bar)
{
	assert(status_bar);
	QObject::connect(progress_bar, &QObject::destroyed, this, &QStatusBarProgress::onBarDeleted);
	QObject::connect(this, &QStatusBarProgress::sigShow, this, &QStatusBarProgress::onShow);
	QObject::connect(this, &QStatusBarProgress::sigClose, this, &QStatusBarProgress::onClose);
	status_bar->addPermanentWidget(progress_bar);
}
void isis::qt5::QStatusBarProgress::show(size_t max, std::string header)
{
	emit sigShow(max,QString::fromStdString(header));
}
void isis::qt5::QStatusBarProgress::close()
{
	emit sigClose();
}
void isis::qt5::QStatusBarProgress::onBarDeleted(QObject *p)
{
	assert(p== nullptr || status_bar==p);
	status_bar = nullptr;
	close();
}
void isis::qt5::QStatusBarProgress::onShow(quint64 max, QString header)
{
	if(status_bar){
		status_bar->showMessage(header);
		progress_bar->show();
	} else
		LOG(Debug,error) << "Calling show on status bar that was deleted";
	QProgressBarWrapper::show(max, header.toStdString());
}
void isis::qt5::QStatusBarProgress::onClose()
{
	if(status_bar){
		status_bar->clearMessage();
		progress_bar->hide();
	} else
		LOG(Debug,error) << "Calling close on status bar that was deleted";
	QProgressBarWrapper::close();
}

isis::qt5::QGroupBoxProgress::QGroupBoxProgress(bool autohide, QWidget* parent): QGroupBox(parent), QProgressBarWrapper(new QProgressBar(this)), show_always(!autohide)
{
	if(!autohide)
		QWidget::show();
	setLayout(new QHBoxLayout());
	layout()->addWidget(progress_bar);
}
void isis::qt5::QGroupBoxProgress::close()
{
	QGroupBox::setProperty("visible",true);
	QProgressBarWrapper::close();
}
void isis::qt5::QGroupBoxProgress::show(size_t max, std::string header)
{
	QGroupBox::setProperty("title",QString::fromStdString(header));
	QGroupBox::setProperty("visible",true);
	QProgressBarWrapper::show(max,header);//this will display the header inside the progressbar
}
