//
// Created by enrico on 22.08.20.
//

#pragma once
#include <stddef.h>
#include <map>
#include <isis/core/propmap.hpp>

namespace isis::io{
std::pair<std::string, util::PropertyMap::PropPath> &tag(uint32_t id);
std::optional<std::pair<std::string, util::PropertyMap::PropPath>> query_tag(uint32_t id);
bool known_tag(uint32_t id);
}
