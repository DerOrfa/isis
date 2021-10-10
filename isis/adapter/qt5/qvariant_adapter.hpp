#pragma once

#include "common.hpp"
#include <QVariant>

namespace isis::qt5{
	QVariant makeQVariant(const isis::util::Value &val);
}