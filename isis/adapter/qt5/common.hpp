#pragma once

#include <QImage>
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
}
}
