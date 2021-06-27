//
// Created by enrico on 27.06.21.
//

#pragma once

#include <QAbstractListModel>
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

class QLogStore : public QAbstractListModel
{
	Q_OBJECT
	QList<LogEvent> events;
public:
	int columnCount(const QModelIndex &parent) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	[[nodiscard]] int rowCount(const QModelIndex &parent) const override;
	[[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
	void add(const LogEvent &event);
	bool isAttached();
};
}