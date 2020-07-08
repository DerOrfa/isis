/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2020  <copyright holder> <email>
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
 */

#pragma once

#include "valuearray.hpp"

namespace isis::data{
template<typename TYPE> class TypedArray :  public ValueArrayNew
{
	std::shared_ptr<TYPE> &me;
public:
	TypedArray(const ValueArrayNew& other):ValueArrayNew(other),me(castTo<TYPE>()){}
	TypedArray(size_t length):TypedArray(ValueArrayNew::make<TYPE>(length)){}

	TypedArray(std::shared_ptr<TYPE> ptr, size_t length):ValueArrayNew(ptr,length),me(castTo<TYPE>()){}
	TypedArray():TypedArray(std::shared_ptr<TYPE>(),0){}//creates an invalid array
	TYPE* begin(){return me.get();}
	TYPE* end(){return me.get()+getLength();}
	const TYPE* begin()const{return me.get();}
	const TYPE* end()const{return me.get()+getLength();}
	TypedArray<TYPE>& operator=(const TypedArray<TYPE>& other){
		ValueArrayNew::operator=(other);
		//"me" will still reference my own ValueArrayNew::std::shared_ptr<TYPE> which got overwritten by the copy operation above
		return *this;
	}
};
}
