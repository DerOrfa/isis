#pragma once

#include <QStringList>
#include "../../core/image.hpp"

namespace isis{
	struct Qt5Log   {static constexpr char name[]="Qt5";      static constexpr bool use = _ENABLE_LOG;};
	struct Qt5Debug {static constexpr char name[]="Qt5Debug"; static constexpr bool use = _ENABLE_DEBUG;};

namespace qt5{

	typedef Qt5Debug Debug;
	typedef Qt5Log Runtime;

	template<typename HANDLE> void enableLog( LogLevel level )
	{
		ENABLE_LOG( Qt5Log, HANDLE, level );
		ENABLE_LOG( Qt5Debug, HANDLE, level );
	}

	template<typename T>
	QStringList slist_to_QStringList(const std::list<T> &list)
	{
		QStringList ret;
		std::transform(
			list.begin(),list.end(),
			std::back_inserter(ret),
			[](const T &f){return f.c_str();}
		);
		return ret;
	}
	template<typename T>
	std::list<T> QStringList_to_slist(const QStringList &list)
	{
		std::list<T> ret;
		std::transform(
			list.begin(),list.end(),
			std::back_inserter(ret),
			[](const QString &f){return f.toStdString().c_str();}
		);
		return ret;
	}
}
}
