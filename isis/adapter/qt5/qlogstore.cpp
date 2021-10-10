#include "qlogstore.hpp"
#include <QBrush>
#include <QMetaMethod>
#include <iostream>
#include <cassert>

isis::qt5::LogEvent::LogEvent(const isis::util::Message& msg)
{
	m_file = QString::fromStdString(msg.m_file.native());
	m_level = msg.m_level;
	m_line = msg.m_line;
	m_module = QString::fromStdString(msg.m_module);
	m_object = QString::fromStdString(msg.m_object);
	m_timeStamp.setTime_t(msg.m_timeStamp);
	m_unformatted_msg = QString::fromStdString(msg.str());

	std::transform(
		msg.m_subjects.begin(),msg.m_subjects.end(),
		std::back_inserter(m_subjects),
		[](const std::string &s){return QString::fromStdString(s);}
	);
}
QString isis::qt5::LogEvent::merge()const
{
	QString ret= m_unformatted_msg;

	for(const auto &subj:m_subjects)
		ret.replace(QString("{s}"),subj);
	return  ret;
}

void isis::qt5::QLogStore::add(const isis::qt5::LogEvent &event)
{
	static const std::map<LogLevel,Qt::GlobalColor> color_map={
		{error,Qt::red},
		{warning,Qt::yellow},
		{notice,Qt::green},
		{info,Qt::gray},
		{verbose_info,Qt::gray}
	};
	static const std::map<LogLevel,QString> severity_map={
		{error,"error"},
		{warning,"warning"},
		{notice,"notice"},
		{info,"info"},
		{verbose_info,"verbose"}
	};
	QList<QStandardItem*> items{
		new QStandardItem(),
		new QStandardItem(severity_map.at(event.m_level)),
		new QStandardItem(event.m_module),
		new QStandardItem(event.merge())
	};
	items[0]->setData(event.m_timeStamp,Qt::DisplayRole);

	const QBrush color(color_map.at(event.m_level));
	for(QStandardItem* itm:items)
		itm->setData(color,Qt::ForegroundRole);

	appendRow(items);
	emit notify(event.m_level);
}
bool isis::qt5::QLogStore::isAttached()
{
	return m_attached;
}
void isis::qt5::QLogStore::connectNotify(const QMetaMethod &signal)
{
	if(signal == QMetaMethod::fromSignal(&QAbstractListModel::rowsInserted)){
		++m_attached;
	}
	QStandardItemModel::connectNotify(signal);
}
void isis::qt5::QLogStore::disconnectNotify(const QMetaMethod &signal)
{
	assert(m_attached>0);
	if(signal == QMetaMethod::fromSignal(&QAbstractListModel::rowsInserted)){
		--m_attached;
	}
	QStandardItemModel::connectNotify(signal);
}
isis::qt5::QLogStore::QLogStore(QObject *parent) : QStandardItemModel(parent)
{
	setHorizontalHeaderLabels({"timestamp","severity","module","message"});
}
