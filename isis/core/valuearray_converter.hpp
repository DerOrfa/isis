/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <memory>
#include <map>
#include "value.hpp"

/// @cond _internal

namespace isis::data
{

class ValueArray;
struct scaling_pair;

class ValueArrayConverterBase
{
public:
	virtual void convert(const ValueArray &src, ValueArray &dst, const scaling_pair &scaling )const;
	virtual ValueArray generate(const ValueArray &src, const scaling_pair &scaling )const = 0;//@todo replace by create+copy
	virtual ValueArray create(size_t len )const = 0;
	virtual scaling_pair getScaling(const util::Value &min, const util::Value &max )const;
	static std::shared_ptr<const ValueArrayConverterBase> get() {return std::shared_ptr<const ValueArrayConverterBase>();}
	virtual ~ValueArrayConverterBase() {}
};

API_EXCLUDE_BEGIN;
namespace _internal
{
typedef std::shared_ptr<const ValueArrayConverterBase> ConverterPtr;
typedef std::map< size_t, std::map<size_t, ConverterPtr> > ConverterMap;


class ValueArrayConverterMap : public ConverterMap
{
public:
	ValueArrayConverterMap();
};

}
API_EXCLUDE_END;
}

/// @endcond _internal
