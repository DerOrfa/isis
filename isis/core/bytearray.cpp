/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2017  <copyright holder> <email>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "bytearray.hpp"

namespace isis{
namespace data{

ByteArray::ByteArray(const std::shared_ptr<uint8_t>& ptr, size_t length):TypedArray<uint8_t>(ValueArrayNew(ptr,length)){}
ByteArray::ByteArray(size_t length):TypedArray<uint8_t>(make<uint8_t>(length)){}

ByteArray::GeneratorMap::GeneratorMap()
{
// 	boost::mpl::for_each<util::_internal::types>( proc( this ) );
	assert( false );
}
}
}
