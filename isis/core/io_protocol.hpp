//
// Created by reimer on 7/13/22.
//

#pragma once

#include "common.hpp"
#include "types.hpp"

namespace isis::io
{
class IoProtocol
{
	virtual util::slist prefixes() = 0;
};
}
