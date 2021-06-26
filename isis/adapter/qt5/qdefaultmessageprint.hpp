#pragma once

#include "../../core/message.hpp"
#include <QString>
#include <QObject>
#include <QDateTime>

namespace isis::qt5
{

class LogEvent
{
public:
	LogEvent()=default;
	explicit LogEvent(const util::Message &msg);
	QString merge();
	QString m_object, m_module;
	QString m_file;
	QStringList m_subjects;
	QDateTime m_timeStamp;
	int m_line;
	LogLevel m_level;
	QString m_unformatted_msg;
};

class LogEventList : public std::list<LogEvent> {};


class QDefaultMessageHandler : public QObject, public util::MessageHandlerBase
{
	Q_OBJECT
public:
Q_SIGNALS:
	void commitMessage( qt5::LogEvent message );

public:
	void commit( const util::Message &msg ) override;
	void qmessageBelow( LogLevel level );
	explicit QDefaultMessageHandler( LogLevel level );
	~QDefaultMessageHandler() override;
private:
	LogLevel m_PushMsgBoxLogLevel;

};

}
