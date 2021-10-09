//
// Created by enrico on 27.06.21.
//

#pragma once

#include <QStandardItemModel>
#include <QDateTime>
#include "qdefaultmessageprint.hpp"

namespace isis::qt5
{

class LogEvent
{
public:
	LogEvent()=default;
	LogEvent(const util::Message &msg);
	QString merge()const;
	QString m_object, m_module;
	QString m_file;
	QStringList m_subjects;
	QDateTime m_timeStamp;
	int m_line;
	LogLevel m_level;
	QString m_unformatted_msg;
};

class QLogStore : public QStandardItemModel
{
	Q_OBJECT
	QList<LogEvent> events;
	size_t m_attached=0;
public:
	QLogStore(QObject *parent= nullptr);
	void add(const LogEvent &event);
	bool isAttached();
Q_SIGNALS:
	void notify(int);
protected:
	void connectNotify(const QMetaMethod &signal) override;
	void disconnectNotify(const QMetaMethod &signal) override;
};
}