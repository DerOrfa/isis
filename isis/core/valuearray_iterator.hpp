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

#include "value.hpp"

namespace isis::data::_internal{

class ConstValueAdapter
{
	template<bool BB> friend class GenericValueIterator; //allow the iterators (and only them) to create the adapter
public:
	typedef const util::Value ( *Getter )(const void * );
	typedef void ( *Setter )( void *, const util::Value & );
protected:
	const Getter getter;
	const uint8_t *const p;
	ConstValueAdapter( const uint8_t *const _p, Getter _getValueFunc );
public:
	// to make some algorithms work
	bool operator==( const util::Value &val )const;
	bool operator!=( const util::Value &val )const;
	//@make ValueAdapter implicitly convertible to Value
	bool operator==( const ConstValueAdapter &val )const;
	bool operator!=( const ConstValueAdapter &val )const;

	bool operator<( const util::Value &val )const;
	bool operator>( const util::Value &val )const;
	//@make ValueAdapter implicitly convertible to Value
	bool operator<( const ConstValueAdapter &val )const;
	bool operator>( const ConstValueAdapter &val )const;

	const std::unique_ptr<util::Value> operator->() const; //maybe more trouble than worth it
	const std::string toString( bool label = false )const;
	operator util::Value()const{return getter(p);}
};
class WritingValueAdapter: public ConstValueAdapter
{
	Setter setValueFunc;
	size_t byteSize;
	template<bool BB> friend class GenericValueIterator; //allow the iterators (and only them) to create the adapter
protected:
	WritingValueAdapter( uint8_t *const _p, Getter _getValueFunc, Setter _setValueFunc, size_t _size );
public:
	WritingValueAdapter operator=( const util::Value &val );
	void swapwith( const WritingValueAdapter &b )const; // the WritingValueAdapter is const not what its dereferencing
};

template<bool IS_CONST> class GenericValueIterator
{
	typedef typename std::conditional<IS_CONST, const uint8_t *, uint8_t *>::type ptr_type;
	ptr_type p, start; //we need the starting position for operator[]
	size_t byteSize;
	ConstValueAdapter::Getter getValueFunc;
	ConstValueAdapter::Setter setValueFunc;
	friend class GenericValueIterator<true>; //yes, I'm my own friend, sometimes :-) (enables the constructor below)
public:
	using iterator_category = std::random_access_iterator_tag;
	using value_type = typename std::conditional_t<IS_CONST, ConstValueAdapter, WritingValueAdapter>;
	using difference_type = ptrdiff_t;
	using pointer = std::conditional_t<IS_CONST, ConstValueAdapter, WritingValueAdapter>;
	using reference = std::conditional_t<IS_CONST, ConstValueAdapter, WritingValueAdapter>;
	using ThisType = GenericValueIterator<IS_CONST>;
	
	//will become additional constructor from non const if this is const, otherwise overrride the default copy contructor
	GenericValueIterator( const GenericValueIterator<false> &src ): 
	    p( src.p ), start( src.start ), byteSize( src.byteSize ), getValueFunc( src.getValueFunc ), setValueFunc( src.setValueFunc ) {}
	GenericValueIterator() = delete;
	GenericValueIterator( ptr_type _p, ptr_type _start, size_t _byteSize, ConstValueAdapter::Getter _getValueFunc, ConstValueAdapter::Setter _setValueFunc ):
	    p( _p ), start( _start ), byteSize( _byteSize ), getValueFunc( _getValueFunc ), setValueFunc( _setValueFunc )
	{}

	difference_type getMyDistance()const{
        return std::distance(start,p)/byteSize;
	}

	ThisType& operator++() {p += byteSize; return *this;}
	ThisType& operator--() {p -= byteSize; return *this;}

	ThisType operator++( int ) {ThisType tmp = *this; ++*this; return tmp;}
	ThisType operator--( int ) {ThisType tmp = *this; --*this; return tmp;}

	reference operator*() const;
	pointer  operator->() const {return operator*();}

	bool operator==( const ThisType& cmp )const {return p == cmp.p;}
	bool operator!=( const ThisType& cmp )const {return !( *this == cmp );}

	bool operator>( const ThisType &cmp )const {return p > cmp.p;}
	bool operator<( const ThisType &cmp )const {return p < cmp.p;}

	bool operator>=( const ThisType &cmp )const {return p >= cmp.p;}
	bool operator<=( const ThisType &cmp )const {return p <= cmp.p;}

	difference_type operator-( const ThisType &cmp )const {return ( p - cmp.p ) / byteSize;}

	ThisType operator+( typename ThisType::difference_type n )const
	{return ( ThisType( *this ) += n );}
	ThisType operator-( typename ThisType::difference_type n )const
	{return ( ThisType( *this ) -= n );}


	ThisType &operator+=( typename ThisType::difference_type n )
	{p += ( n * byteSize ); return *this;}
	ThisType &operator-=( typename ThisType::difference_type n )
	{p -= ( n * byteSize ); return *this;}

	typename ThisType::reference operator[]( typename ThisType::difference_type n )const {
		//the book says it has to be the n-th elements of the whole object, so we have to start from what is hopefully the beginning
		return *( ThisType( start, start, byteSize, getValueFunc, setValueFunc ) += n );
	}

};
//specialise the dereferencing operators. They have to return (and create) different objects with different constructors
template<> GenericValueIterator<true>::reference GenericValueIterator<true>::operator*() const;
template<> GenericValueIterator<false>::reference GenericValueIterator<false>::operator*() const;

/**
 * Basic iterator for ValueArray.
 * This is a common iterator following the random access iterator model.
 * It is not part of the reference counting used in ValueArray. So make sure you keep the ValueArray you created it from while you use this iterator.
 */
template<typename TYPE> class TypedArrayIterator
{
	TYPE *p;
public:
	using iterator_category = std::random_access_iterator_tag;
	using value_type = TYPE;
	using difference_type = ptrdiff_t;
	using pointer = TYPE*;
	using reference = TYPE&;
	
	friend TypedArrayIterator<typename std::add_const_t<TYPE> >;

	TypedArrayIterator(): p(nullptr) {}
	explicit TypedArrayIterator( TYPE *_p ): p( _p ) {}
	TypedArrayIterator( const TypedArrayIterator<typename std::remove_const<TYPE>::type > &src ): p( src.p ) {}

	TypedArrayIterator<TYPE>& operator++() {++p; return *this;}
	TypedArrayIterator<TYPE>& operator--() {--p; return *this;}

	TypedArrayIterator<TYPE>  operator++( int ) {TypedArrayIterator<TYPE> tmp = *this; ++*this; return tmp;}
	TypedArrayIterator<TYPE>  operator--( int ) {TypedArrayIterator<TYPE> tmp = *this; --*this; return tmp;}

	reference operator*() const { return *p; }
	pointer operator->() const { return p; }

	bool operator==( const TypedArrayIterator<TYPE> &cmp )const {return p == cmp.p;}
	bool operator!=( const TypedArrayIterator<TYPE> &cmp )const {return !( *this == cmp );}

	bool operator>( const TypedArrayIterator<TYPE> &cmp )const {return p > cmp.p;}
	bool operator<( const TypedArrayIterator<TYPE> &cmp )const {return p < cmp.p;}

	bool operator>=( const TypedArrayIterator<TYPE> &cmp )const {return p >= cmp.p;}
	bool operator<=( const TypedArrayIterator<TYPE> &cmp )const {return p <= cmp.p;}

	TypedArrayIterator<TYPE> operator+( difference_type n )const {return TypedArrayIterator<TYPE>( p + n );}
	TypedArrayIterator<TYPE> operator-( difference_type n )const {return TypedArrayIterator<TYPE>( p - n );}

	difference_type operator-( const TypedArrayIterator<TYPE> &cmp )const {return p - cmp.p;}

	TypedArrayIterator<TYPE> &operator+=( difference_type n ) {p += n; return *this;}
	TypedArrayIterator<TYPE> &operator-=( difference_type n ) {p -= n; return *this;}

	reference operator[]( difference_type n )const {return *( p + n );}

	operator pointer(){return p;}
};
}
/// @cond _internal
namespace std
{
    void swap(const isis::data::_internal::WritingValueAdapter &a,const isis::data::_internal::WritingValueAdapter &b);
	/// Streaming output for ConstValueAdapter (use it as a const Value)
	template<typename charT, typename traits> basic_ostream<charT, traits>&
	operator<<( basic_ostream<charT, traits> &out, const isis::data::_internal::ConstValueAdapter &v )
	{
		return out << isis::util::Value(v);
	}
}
/// @endcond _internal

