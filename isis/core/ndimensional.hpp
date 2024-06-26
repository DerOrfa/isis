//
// C++ Interface: ndimensional
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#pragma once

#define __need_size_t
#include <cstddef>
#include <algorithm>
#include <string>
#include "common.hpp"
#include "stringop.hpp"
#include "vector.hpp"
#include "progressfeedback.hpp"

namespace isis::data
{

/// Base class for anything that has dimensional size
template<unsigned short DIMS> class NDimensional
{
	std::array<size_t,DIMS> m_dim;
	constexpr size_t _dimStride( unsigned short dim )const
	{
		return dim ? _dimStride( dim-1 ) * m_dim[dim - 1]:1;
	}
	constexpr size_t _dim2index( const std::array<size_t,DIMS> &d, unsigned short DIM )const
	{
		return DIM ?
			d[DIM] * _dimStride( DIM ) + _dim2index( d, DIM - 1 ):
			d[0]   * _dimStride( 0 );
	}
	constexpr std::array<size_t,DIMS> _index2dim( const size_t index, unsigned short DIM, size_t vol )const
	{
		if(DIM){
			std::array<size_t,DIMS> ret=_index2dim( index % vol, DIM - 1, vol / m_dim[DIM - 1] );
			ret[DIM] = index / vol;
			return ret;
		} else {
			assert(vol == 1);
			return {index};
		}
	}

protected:
	static constexpr size_t dims = DIMS;
	NDimensional() = default;
public:
	/**
	 * Initializes the size-vector.
	 * This must be done before anything else, or behaviour will be undefined.
	 * \param d array with sizes to use. (d[0] is most iterating element / lowest dimension)
	 */
	void init(const std::array<size_t,DIMS> &d ) {m_dim = d;}
	NDimensional( const NDimensional &src ) = default;
	/**
	 * Compute linear index from n-dimensional index,
	 * \param coord array of indexes (d[0] is most iterating element / lowest dimension)
	 */
	size_t getLinearIndex( const std::array<size_t,DIMS> &coord )const {
		return _dim2index( coord, DIMS - 1 );
	}
	/**
	 * Compute coordinates from linear index,
	 * \param coord array to put the computed coordinates in (d[0] will be most iterating element / lowest dimension)
	 * \param index the linear index to compute the coordinates from
	 */
	util::vector<size_t,DIMS> getCoordsFromLinIndex( const size_t index )const {
		return util::vector<size_t,DIMS>::fromArray(_index2dim( index, DIMS - 1, getVolume() / m_dim[DIMS - 1] ));
	}
	/**
	 * Check if index fits into the dimensional size of the object.
	 * \param coord index to be checked (d[0] is most iterating element / lowest dimension)
	 * \returns true if given index will get a reasonable result when used for getLinearIndex
	 */
	constexpr bool isInRange( const std::array<size_t,DIMS> &coord )const {
		for(unsigned short i=0;i<DIMS;i++){
			if(coord[i]>=m_dim[i])
				return false;
		}
		return true;
	}
	/**
	 * Get the size of the object in elements of TYPE.
	 * \returns \f$ \prod_{i=0}^{DIMS-1} getDimSize(i) \f$
	 */
	[[nodiscard]] size_t getVolume()const {
		return _dimStride( DIMS );
	}
	///\returns the size of the object in the given dimension
	[[nodiscard]] size_t getDimSize( size_t idx )const {
		return m_dim[idx];
	}

	/// generates a string representing the size
	std::string getSizeAsString( const char *delim = "x" )const {
		return util::listToString( std::begin(m_dim), std::end(m_dim), delim, "", "" );
	}

	/// generates a FixedVector\<DIMS\> representing the size
	util::vector<size_t,DIMS> getSizeAsVector()const {
		return util::vector<size_t,DIMS>::fromArray(m_dim);
	}

	/**
	 * get amount of relevant dimensions (last dim with size>1)
	 * e.g. on a slice (1x64x1x1) it will be 2
	 * Therefore getDimSize(getRelevantDims()-1) will be 64
	 */
	size_t getRelevantDims()const {
		size_t ret = 0;

		for ( unsigned short i = DIMS; i; i-- ) {
			if ( m_dim[i - 1] > 1 ) {
				ret = i;
				break;
			}
		}

		return ret;
	}
	util::vector<float, DIMS> getFoV( const util::vector<float, DIMS> &voxelSize, const util::vector<float, DIMS> &voxelGap )const {
		LOG_IF( getVolume() == 0, DataLog, warning ) << "Calculating FoV of empty data";
		util::vector<size_t, DIMS> voxels = getSizeAsVector();
		util::vector<size_t, DIMS> gaps = voxels;
		for(size_t &v:gaps)
			--v;

		return voxelSize * voxels + voxelGap * gaps;
	}
	
	template<typename ITER> void swapDim(size_t dim_a,size_t dim_b,ITER at, std::shared_ptr<util::ProgressFeedback> feedback=std::shared_ptr<util::ProgressFeedback>()){
		std::vector<bool> visited(getVolume());
		data::NDimensional<DIMS> oldshape=*this;

		//reshape myself
		std::swap(m_dim[dim_a],m_dim[dim_b]);
		ITER cycle = at,last=cycle+getVolume();
		if(feedback)
			feedback->show(getVolume()-1, std::string("Swapping ")+std::to_string(getVolume())+" voxels");

		while(++cycle != last){
			if(feedback) 
				feedback->progress();
			size_t i=cycle-at;
			if(visited[i])continue;
			
			do{
				std::array<size_t,DIMS> currIndex=oldshape.getCoordsFromLinIndex(i);
				std::swap(currIndex[dim_a],currIndex[dim_b]);
				i=getLinearIndex(currIndex);
				std::swap(*(at+i),*cycle);
				visited[i] = true;
			} while (at+i != cycle);
		}
	}
};

}


