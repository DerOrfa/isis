#pragma once

#include <iterator>
#include <memory>
#include <vector>

namespace isis::data{
namespace _internal
{
/**
 * Generic iterator for voxels in Images.
 * It automatically jumps from chunk to chunk.
 * It needs the chunks and the image to be there for to work properly (so don't delete the image, and dont reIndex it),
 * It assumes that all Chunks have the same size (which is a rule for Image as well, so this should be given)
 */
template<typename ARRAY_TYPE, bool IS_CONST> class ImageIteratorTemplate
{
protected:
	using array_type = ARRAY_TYPE;
	using ThisType = ImageIteratorTemplate<ARRAY_TYPE,IS_CONST> ;
	using inner_iterator = typename std::conditional_t<IS_CONST, typename array_type::const_iterator, typename array_type::iterator>;
public:
	using iterator_category = std::random_access_iterator_tag;
	using value_type = typename inner_iterator::value_type;
	using difference_type = typename inner_iterator::difference_type;
	using pointer = typename inner_iterator::pointer;
	using reference = typename inner_iterator::reference;

	//we have to use the non-const here, otherwise the iterator would not be convertible into const_iterator
	std::shared_ptr<std::vector<array_type>> chunks;

	size_t ch_idx;
	inner_iterator current_it;
	typename inner_iterator::difference_type ch_len;

	typename inner_iterator::difference_type currentDist() const {
		if ( ch_idx >= chunks->size() )
			return 0; // if we're behind the last chunk assume we are at the "start" of the "end"-chunk
		else {
			//because we're in a const function [] will get us a const array_type even if the iterator shouldn't. So we cast it back to the proper const-state
			const inner_iterator chit_begin = const_cast<array_type&>(chunks->operator[](ch_idx)).begin();
			const auto distance = std::distance ( chit_begin, current_it );
			assert(distance<ch_len);
			return distance; 
		}
	}
	friend class ImageIteratorTemplate<ARRAY_TYPE,true>; //yes, I'm my own friend, sometimes :-) (enables the constructor below)
public:
	//will become additional constructor from non const if this is const, otherwise override the default copy constructor
	ImageIteratorTemplate ( const ImageIteratorTemplate<ARRAY_TYPE,false> &src ) :
		chunks ( src.chunks ), ch_idx ( src.ch_idx ),
		current_it ( src.current_it ),
		ch_len ( src.ch_len )
	{}

	// empty constructor
	ImageIteratorTemplate() : ch_idx ( 0 ), ch_len ( 0 ) {}


	// normal constructor
	explicit ImageIteratorTemplate (const std::vector<array_type>&_chunks) :
		chunks ( std::make_shared<std::vector<array_type>>(_chunks) ), ch_idx ( 0 ),
		current_it ( chunks->operator[](0).begin() ),
		ch_len ( chunks->operator[](0).getLength() )
	{}

	ThisType &operator++() {
		return operator+= ( 1 );
	}
	ThisType &operator--() {
		return operator-= ( 1 );
	}

	ThisType operator++ ( int ) {
		ThisType tmp = *this;
		operator++();
		return tmp;
	}
	ThisType operator-- ( int ) {
		ThisType tmp = *this;
		operator--();
		return tmp;
	}

	reference operator*() const {
		return current_it.operator * ();
	}
	pointer operator->() const {
		return current_it.operator->();
	}
	operator pointer(){
		return current_it.operator->();
	}


	bool operator== ( const ThisType &cmp ) const {
		return ch_idx == cmp.ch_idx && current_it == cmp.current_it;
	}
	bool operator!= ( const ThisType &cmp ) const {
		return !operator== ( cmp );
	}

	bool operator> ( const ThisType &cmp ) const {
		return ch_idx > cmp.ch_idx || ( ch_idx == cmp.ch_idx && current_it > cmp.current_it );
	}
	bool operator< ( const ThisType &cmp ) const {
		return ch_idx < cmp.ch_idx || ( ch_idx == cmp.ch_idx && current_it < cmp.current_it );
	}


	bool operator>= ( const ThisType &cmp ) const {
		return operator> ( cmp ) || operator== ( cmp );
	}
	bool operator<= ( const ThisType &cmp ) const {
		return operator< ( cmp ) || operator== ( cmp );
	}

	typename inner_iterator::difference_type operator- ( const ThisType &cmp ) const {
		typename inner_iterator::difference_type dist = ( ch_idx - cmp.ch_idx ) * ch_len; // get the (virtual) distance from my current block to cmp's current block

		if ( ch_idx >= cmp.ch_idx ) { //if I'm beyond cmp add my current pos to the distance, and substract his
			dist += currentDist() - cmp.currentDist();
		} else {
			dist += cmp.currentDist() - currentDist();
		}

		return dist;
	}

	ThisType operator+ ( typename ThisType::difference_type n ) const {
		return ThisType ( *this ) += n;
	}
	ThisType operator- ( typename ThisType::difference_type n ) const {
		return ThisType ( *this ) -= n;
	}

	ThisType &operator+= ( typename inner_iterator::difference_type n ) {
		n += currentDist(); //start from current begin (add current_it-(begin of the current chunk) to n)
		assert ( ( n / ch_len + static_cast<typename ThisType::difference_type> ( ch_idx ) ) >= 0 );
		ch_idx += n / ch_len; //if neccesary jump to next chunk

		if ( ch_idx < chunks->size() )
			current_it = chunks->operator[](ch_idx).begin() + n % ch_len; //set new current iterator in new chunk plus the "rest"
		else
			current_it = chunks->back().end() ; //set current_it to the last chunks end iterator if we are behind it

		//@todo will break if ch_cnt==0

		return *this;
	}
	ThisType &operator-= ( typename inner_iterator::difference_type n ) {
		return operator+= ( -n );
	}

};
}
}
