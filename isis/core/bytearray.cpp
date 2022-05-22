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
#include "color.hpp"

namespace isis::data{

namespace _internal {

typedef std::function<data::ValueArray(data::ByteArray &, size_t, size_t, bool )> generator_fn;

template<int ID=std::variant_size_v<ArrayTypes>-1>
void make_generators(std::map<unsigned short, generator_fn>& map) { //The ID here is the index in ArrayTypes, NOT the isis-typeID
	typedef typename std::variant_alternative_t<ID,ArrayTypes>::element_type element_type; //get the arrays' element type
	constexpr size_t index=util::typeID<element_type>();//get the isis-typeID of the element type

	map[index]=[]( data::ByteArray &bytes, size_t offset, size_t len, bool swap_endianess ){
		    return bytes.at<element_type>( offset, len, swap_endianess );
	};
	make_generators<ID-1>(map);//recursion
}
template<> void make_generators<-1>(std::map<unsigned short, generator_fn>&) {}//terminator

struct GeneratorMap: public std::map<unsigned short, generator_fn> {
	GeneratorMap(){
		make_generators(*this);
	}
};


}

ByteArray::ByteArray(const std::shared_ptr<uint8_t>& ptr, size_t length):TypedArray<uint8_t>(ValueArray(ptr, length)){}
ByteArray::ByteArray(size_t length):TypedArray<uint8_t>(make<uint8_t>(length)){}
ByteArray::ByteArray(const TypedArray<uint8_t> &ref):TypedArray<uint8_t>(ref){}

ValueArray ByteArray::atByID(unsigned short ID, std::size_t offset, std::size_t len, bool swap_endianess)
{
	LOG_IF(!isValid(), Debug, error ) << "There is no mapped data for this ByteArray - I'm very likely gonna crash soon ..";
	_internal::GeneratorMap &map = util::Singletons::get<_internal::GeneratorMap, 0>();
	assert( !map.empty() );
	const _internal::generator_fn gen = map[ID];
	assert( gen );
	return gen( *this, offset, len, swap_endianess );
}


}
