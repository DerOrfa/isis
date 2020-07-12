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
	typedef const util::ValueNew ( *Getter )( const void * );
	typedef void ( *Setter )( void *, const util::ValueNew & );
protected:
	const Getter getter;
	const uint8_t *const p;
	ConstValueAdapter( const uint8_t *const _p, Getter _getValueFunc );
public:
	// to make some algorithms work
	bool operator==( const util::ValueNew &val )const;
	bool operator!=( const util::ValueNew &val )const;
	//@make ValueAdapter implicitly convertible to ValueNew
	bool operator==( const ConstValueAdapter &val )const;
	bool operator!=( const ConstValueAdapter &val )const;

	bool operator<( const util::ValueNew &val )const;
	bool operator>( const util::ValueNew &val )const;
	//@make ValueAdapter implicitly convertible to ValueNew
	bool operator<( const ConstValueAdapter &val )const;
	bool operator>( const ConstValueAdapter &val )const;

	const util::ValueNew operator->() const;
	const std::string toString( bool label = false )const;
};
class WritingValueAdapter: public ConstValueAdapter
{
	Setter setValueFunc;
	size_t byteSize;
	template<bool BB> friend class GenericValueIterator; //allow the iterators (and only them) to create the adapter
protected:
	WritingValueAdapter( uint8_t *const _p, Getter _getValueFunc, Setter _setValueFunc, size_t _size );
public:
	WritingValueAdapter operator=( const util::ValueNew &val );
	void swapwith( const WritingValueAdapter &b )const; // the WritingValueAdapter is const not what its dereferencing
};

template<bool IS_CONST> class GenericValueIterator :
    public std::iterator < std::random_access_iterator_tag,
    typename std::conditional<IS_CONST, ConstValueAdapter, WritingValueAdapter>::type,
    ptrdiff_t,
    typename std::conditional<IS_CONST, ConstValueAdapter, WritingValueAdapter>::type,
    typename std::conditional<IS_CONST, ConstValueAdapter, WritingValueAdapter>::type
    >
{
	typedef typename std::conditional<IS_CONST, const uint8_t *, uint8_t *>::type ptr_type;
	ptr_type p, start; //we need the starting position for operator[]
	size_t byteSize;
	ConstValueAdapter::Getter getValueFunc;
	ConstValueAdapter::Setter setValueFunc;
	friend class GenericValueIterator<true>; //yes, I'm my own friend, sometimes :-) (enables the constructor below)
public:
	GenericValueIterator( const GenericValueIterator<false> &src ): //will become additional constructor from non const if this is const, otherwise overrride the default copy contructor
	    p( src.p ), start( src.p ), byteSize( src.byteSize ), getValueFunc( src.getValueFunc ), setValueFunc( src.setValueFunc ) {}
	GenericValueIterator(): p( NULL ), start( p ), byteSize( 0 ), getValueFunc( NULL ), setValueFunc( NULL ) {}
	GenericValueIterator( ptr_type _p, ptr_type _start, size_t _byteSize, ConstValueAdapter::Getter _getValueFunc, ConstValueAdapter::Setter _setValueFunc ):
	    p( _p ), start( _start ), byteSize( _byteSize ), getValueFunc( _getValueFunc ), setValueFunc( _setValueFunc )
	{}

	GenericValueIterator<IS_CONST>& operator++() {p += byteSize; return *this;}
	GenericValueIterator<IS_CONST>& operator--() {p -= byteSize; return *this;}

	GenericValueIterator<IS_CONST> operator++( int ) {GenericValueIterator<IS_CONST> tmp = *this; ++*this; return tmp;}
	GenericValueIterator<IS_CONST> operator--( int ) {GenericValueIterator<IS_CONST> tmp = *this; --*this; return tmp;}

	typename GenericValueIterator<IS_CONST>::reference operator*() const;
	typename GenericValueIterator<IS_CONST>::pointer  operator->() const {return operator*();}

	bool operator==( const GenericValueIterator<IS_CONST>& cmp )const {return p == cmp.p;}
	bool operator!=( const GenericValueIterator<IS_CONST>& cmp )const {return !( *this == cmp );}

	bool operator>( const GenericValueIterator<IS_CONST> &cmp )const {return p > cmp.p;}
	bool operator<( const GenericValueIterator<IS_CONST> &cmp )const {return p < cmp.p;}

	bool operator>=( const GenericValueIterator<IS_CONST> &cmp )const {return p >= cmp.p;}
	bool operator<=( const GenericValueIterator<IS_CONST> &cmp )const {return p <= cmp.p;}

	typename GenericValueIterator<IS_CONST>::difference_type operator-( const GenericValueIterator<IS_CONST> &cmp )const {return ( p - cmp.p ) / byteSize;}

	GenericValueIterator<IS_CONST> operator+( typename GenericValueIterator<IS_CONST>::difference_type n )const
	{return ( GenericValueIterator<IS_CONST>( *this ) += n );}
	GenericValueIterator<IS_CONST> operator-( typename GenericValueIterator<IS_CONST>::difference_type n )const
	{return ( GenericValueIterator<IS_CONST>( *this ) -= n );}


	GenericValueIterator<IS_CONST> &operator+=( typename GenericValueIterator<IS_CONST>::difference_type n )
	{p += ( n * byteSize ); return *this;}
	GenericValueIterator<IS_CONST> &operator-=( typename GenericValueIterator<IS_CONST>::difference_type n )
	{p -= ( n * byteSize ); return *this;}

	typename GenericValueIterator<IS_CONST>::reference operator[]( typename GenericValueIterator<IS_CONST>::difference_type n )const {
		//the book says it has to be the n-th elements of the whole object, so we have to start from what is hopefully the beginning
		return *( GenericValueIterator<IS_CONST>( start, start, byteSize, getValueFunc, setValueFunc ) += n );
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
template<typename TYPE> class TypedArrayIterator: public std::iterator<std::random_access_iterator_tag, TYPE>
{
	TYPE *p;
	typedef typename std::iterator<std::random_access_iterator_tag, TYPE>::difference_type distance;
public:
	TypedArrayIterator(): p( NULL ) {}
	TypedArrayIterator( TYPE *_p ): p( _p ) {}
	TypedArrayIterator( const TypedArrayIterator<typename std::remove_const<TYPE>::type > &src ): p( src.p ) {}

	TypedArrayIterator<TYPE>& operator++() {++p; return *this;}
	TypedArrayIterator<TYPE>& operator--() {--p; return *this;}

	TypedArrayIterator<TYPE>  operator++( int ) {TypedArrayIterator<TYPE> tmp = *this; ++*this; return tmp;}
	TypedArrayIterator<TYPE>  operator--( int ) {TypedArrayIterator<TYPE> tmp = *this; --*this; return tmp;}

	TYPE &operator*() const { return *p; }
	TYPE *operator->() const { return p; }

	bool operator==( const TypedArrayIterator<TYPE> &cmp )const {return p == cmp.p;}
	bool operator!=( const TypedArrayIterator<TYPE> &cmp )const {return !( *this == cmp );}

	bool operator>( const TypedArrayIterator<TYPE> &cmp )const {return p > cmp.p;}
	bool operator<( const TypedArrayIterator<TYPE> &cmp )const {return p < cmp.p;}

	bool operator>=( const TypedArrayIterator<TYPE> &cmp )const {return p >= cmp.p;}
	bool operator<=( const TypedArrayIterator<TYPE> &cmp )const {return p <= cmp.p;}

	TypedArrayIterator<TYPE> operator+( distance n )const {return TypedArrayIterator<TYPE>( p + n );}
	TypedArrayIterator<TYPE> operator-( distance n )const {return TypedArrayIterator<TYPE>( p - n );}

	distance operator-( const TypedArrayIterator<TYPE> &cmp )const {return p - cmp.p;}

	TypedArrayIterator<TYPE> &operator+=( distance n ) {p += n; return *this;}
	TypedArrayIterator<TYPE> &operator-=( distance n ) {p -= n; return *this;}

	TYPE &operator[]( distance n )const {return *( p + n );}

	operator TYPE*(){return p;}
};
}

namespace std
{
    void swap(const isis::data::_internal::WritingValueAdapter &a,const isis::data::_internal::WritingValueAdapter &b);
	/// Streaming output for ConstValueAdapter (use it as a const ValueReference)
	template<typename charT, typename traits> basic_ostream<charT, traits>&
	operator<<( basic_ostream<charT, traits> &out, const isis::data::_internal::ConstValueAdapter &v )
	{
		return out << isis::util::ValueNew(v);
	}
}

