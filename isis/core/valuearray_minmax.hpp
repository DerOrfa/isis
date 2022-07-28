#pragma once

#include <limits>
#include "value.hpp"
#include "../config.hpp"

namespace isis::data::_internal{

/// @cond _internal
API_EXCLUDE_BEGIN;
template<typename T, uint8_t STEPSIZE> std::pair<T, T> calcMinMax( const T *data, size_t len )
{
	static_assert( std::numeric_limits<T>::has_denorm != std::denorm_indeterminate, "denormisation not known" ); //well we're pretty f**ed in this case
	std::pair<T, T> result(
		std::numeric_limits<T>::max(),
		std::numeric_limits<T>::has_denorm ? -std::numeric_limits<T>::max() : std::numeric_limits<T>::min() //for types with denormalization min is _not_ the lowest value
	);
	LOG( Runtime, verbose_info ) << "using generic min/max computation for " << util::typeName<T>();

	for ( const T *i = data; i < data + len; i += STEPSIZE ) {
		if(
			std::numeric_limits<T>::has_infinity &&
			( *i == std::numeric_limits<T>::infinity() || *i == -std::numeric_limits<T>::infinity() )
		)
			continue; // skip this one if its inf

		if ( *i > result.second )result.second = *i; //*i is the new max if it's bigger than the current (gets rid of nan as well)

		if ( *i < result.first )result.first = *i; //*i is the new min if it's smaller than the current (gets rid of nan as well)
	}

	LOG_IF(std::numeric_limits<T>::has_infinity && result.first>result.second,Runtime,warning) 
		<< "Skipped all elements of this array, as they all are inf. Results will be invalid.";

	return result;
}

#ifdef __SSE2__
////////////////////////////////////////////////
// specialize calcMinMax for (u)int(8,16,32)_t /
////////////////////////////////////////////////

template<> std::pair< uint8_t,  uint8_t> calcMinMax< uint8_t, 1>( const  uint8_t *data, size_t len );
template<> std::pair<uint16_t, uint16_t> calcMinMax<uint16_t, 1>( const uint16_t *data, size_t len );
template<> std::pair<uint32_t, uint32_t> calcMinMax<uint32_t, 1>( const uint32_t *data, size_t len );

template<> std::pair< int8_t,  int8_t> calcMinMax< int8_t, 1>( const  int8_t *data, size_t len );
template<> std::pair<int16_t, int16_t> calcMinMax<int16_t, 1>( const int16_t *data, size_t len );
template<> std::pair<int32_t, int32_t> calcMinMax<int32_t, 1>( const int32_t *data, size_t len );
#endif //__SSE2__

struct getMinMaxVisitor { 
	getMinMaxVisitor(size_t len):length(len){}
	std::pair<util::Value, util::Value> minmax;
	size_t length;
	// fallback for unsupported types
	template<typename T> std::enable_if_t<!std::is_arithmetic_v<T>> operator()( const std::shared_ptr<T> &/*ref*/ ) {
		LOG( Debug, error ) << "min/max computation of " << util::typeName<T>() << " is not supported";
	}
	// generic min-max for numbers 
	template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> operator()( const std::shared_ptr<T> &ref ) {
		minmax=calcMinMax<T, 1>( ref.get(), length );
	}
	//specialization for color
	template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> operator()( const std::shared_ptr<util::color<T> > &ref ) {
		std::pair<util::color<T> , util::color<T> > ret;

		for( uint_fast8_t i = 0; i < 3; i++ ) {
			const util::color<T> *col=ref.get();
			const std::pair<T, T> buff = calcMinMax<T, 3>( &col->r + i, length * 3 );
			*( &ret.first.r + i ) = buff.first;
			*( &ret.second.r + i ) = buff.second;
		}
		minmax=ret;
	}
	//specialization for vectors
	template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> operator()( const std::shared_ptr<util::vector3<T> > &ref ) {
		minmax=calcMinMax<T, 1>( ref->data(), length*3 );;
	}
	template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> operator()( const std::shared_ptr<util::vector4<T> > &ref ) {
		minmax=calcMinMax<T, 1>( ref->data(), length*4 );;
	}
	//specialization for complex
	template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> operator()( const std::shared_ptr<std::complex<T> > &ref ) {
		//use compute min/max of magnitude / phase
		auto any_nan= [](const std::complex<T> &v){return std::isnan(v.real()) || std::isnan(v.imag());};
		const auto start=std::find_if_not(ref.get(),ref.get()+length,any_nan);
		if(start==ref.get()+length){
			LOG(Runtime,error) << "Array is all NaN, returning NaN/NaN as minimum/maximum";
			minmax={std::numeric_limits<T>::quiet_NaN(),std::numeric_limits<T>::quiet_NaN()};
		} else {
			T ret_min_sqmag=std::norm(*start),ret_max_sqmag=std::norm(*start);
			
			for(auto i=start;i!=ref.get()+length;i++){
				if(!any_nan(*i)){
					const T &sqmag=std::norm(*i);
					if(ret_min_sqmag>sqmag)ret_min_sqmag=sqmag;
					if(ret_max_sqmag<sqmag)ret_max_sqmag=sqmag;
				}
			}
			minmax=std::make_pair(std::sqrt(ret_min_sqmag),std::sqrt(ret_max_sqmag));
		}
	}
};

/// @endcond
API_EXCLUDE_END;

}
