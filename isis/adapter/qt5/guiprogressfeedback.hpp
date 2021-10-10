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

#pragma once

#include <QObject>
#include "../../core/progressfeedback.hpp"

class QStatusBar;
class QProgressBar;
class QLabel;

namespace isis::qt5{

class QProgressBarWrapper:public QObject, public util::ProgressFeedback
{
	Q_OBJECT
	size_t max=0,current=0;
	int disp_max;
	void update();
public:
	explicit QProgressBarWrapper(int display_max=100);
	void show(size_t max, std::string header) override;
	size_t progress(size_t step) override;
	void close() override;
	size_t getMax() override;
	size_t extend(size_t by) override;
	void restart(size_t new_max) override;
Q_SIGNALS:
	void sigSetVal(int newval);
	void sigShow(QString hdr);
	void sigClose();
};

class QStatusBarProgress : public QProgressBarWrapper{
	Q_OBJECT
	QStatusBar *status_bar;
	QProgressBar *progress_bar;
	QLabel *header_bar;
public:
	explicit QStatusBarProgress(QStatusBar *status_bar);
private Q_SLOTS:
	void onShow(QString header );
	void onClose();
};

}
