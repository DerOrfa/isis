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
	const std::shared_ptr<TYPE> &me;
public:
	using iterator = _internal::TypedArrayIterator<TYPE>;
	using const_iterator = _internal::TypedArrayIterator<const TYPE>;
	using value_reference = typename iterator::reference;
	using const_value_reference = typename const_iterator::reference;
	using value_type=TYPE;

	/// Create a typed value array from a shared ptr of the same type.
	TypedArray(std::shared_ptr<TYPE> ptr, size_t length):ValueArrayNew(ptr,length),me(castTo<TYPE>()){} //we use castTo to set "me" to the ValueArray's shared_ptr (as we know the type)
	/**
	 * Create a typed value array from a ValueArray of any type.
	 * This might use do a type conversion (and thus a deep copy) of the given ValueArray if its type does not fit.
	 */
	TypedArray(const ValueArrayNew& other, const scaling_pair &scaling = scaling_pair()):ValueArrayNew(other.as<TYPE>(scaling)),me(castTo<TYPE>()){
		LOG_IF(!other.is<TYPE>(),Debug,info) << "Made a converted " << typeName() << " copy of a " << other.typeName() << " array";
	}
	/// Create a typed value array from scratch
	TypedArray(size_t length):TypedArray(ValueArrayNew::make<TYPE>(length),scaling_pair(1,0)){} //this will call the constructor for creation from ValueArray but we know we won't need any conversion or scaling
	/// Basic copy constructor
	TypedArray(const TypedArray<TYPE> &ref):TypedArray(static_cast<const ValueArrayNew&>(ref),scaling_pair(1,0)){}//same as above
	TypedArray(TypedArray<TYPE> &&ref)=delete;

	/// Create an invalid array of the correct type.
	TypedArray():TypedArray(std::shared_ptr<TYPE>(),0){}//(makes sure me is valid)

	iterator begin(){return me.get();}
	iterator end(){return me.get()+getLength();}
	const_iterator begin()const{return me.get();}
	const_iterator end()const{return me.get()+getLength();}
	
	TypedArray<TYPE>& operator=(const TypedArray<TYPE>& other){
		ValueArrayNew::operator=(other);
		//"me" will still reference my own ValueArrayNew::std::shared_ptr<TYPE> which got overwritten by the copy operation above
		return *this;
	}
	TypedArray<TYPE>& operator=(TypedArray<TYPE> &&other){
		ValueArrayNew::operator=(other);
		return *this;
	}

	value_reference operator[](size_t at){
		LOG_IF(at>=getLength(),Debug,error) << "Index " << at << " is behind the arrays length (" << getLength() << ")";
		return *(begin()+at);
	}
	const_value_reference operator[](size_t at)const{
		LOG_IF(at>=getLength(),Debug,error) << "Index " << at << " is behind the arrays length (" << getLength() << ")";
		return *(begin()+at);
	}
};
}
