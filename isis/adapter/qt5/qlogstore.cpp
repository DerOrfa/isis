#include "qlogstore.hpp"
#include <QBrush>
#include <iostream>

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

int isis::qt5::QLogStore::rowCount(const QModelIndex &parent) const
{
	return events.count();
}
QVariant isis::qt5::QLogStore::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())return {};
	if (index.row() >= events.count())return {};

	const auto &evt=events[index.row()];
	switch(role){
		case Qt::DisplayRole:
			switch(index.column()){
				case 0:return evt.m_timeStamp;
				case 1:return evt.m_level;
				case 2:return evt.m_module;
				case 3:return evt.merge();
			}
		break;
		case Qt::ForegroundRole:
			switch(evt.m_level){
				case error:return QBrush(Qt::red);
				case warning:return QBrush(Qt::yellow);
				case notice:return QBrush(Qt::green);
				case info:
				case verbose_info:return QBrush(Qt::gray);
			}
		break;
	}

	return {};
}
void isis::qt5::QLogStore::add(const isis::qt5::LogEvent &event)
{
	const auto end=events.count();
	QAbstractListModel::beginInsertRows(QModelIndex(),end,end+1);
	events.push_back(event);
	QAbstractListModel::endInsertRows();
}
bool isis::qt5::QLogStore::isAttached()
{
	return false;
}
int isis::qt5::QLogStore::columnCount(const QModelIndex &parent) const
{
	return 4;
}
QVariant isis::qt5::QLogStore::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation==Qt::Horizontal && role == Qt::DisplayRole){
		switch(section){
			case 0:return "timestamp";
			case 1:return "severity";
			case 2:return "module";
			case 3:return "message";
		}
	}
	return QAbstractListModel::headerData(section,orientation,role);
}
