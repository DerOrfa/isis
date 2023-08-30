#pragma once

#include "types.hpp"

namespace isis::util{

typedef std::variant<
  bool //0
, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t //8
, float, double //10
, color24, color48 //12
, fvector3, dvector3, ivector3 //15
, fvector4, dvector4, ivector4 //18
, ilist, dlist, slist //21
, std::string, isis::util::Selection //23
, std::complex<float>, std::complex<double> //25
, date, timestamp, duration //28
> ValueTypes;

/**
 * Get the id of the type T
 * @tparam T the type for which the id is requested
 * @return the id of the type T if T is a known type
 * @return std::variant_npos otherwise
 */
template<typename T> static constexpr size_t typeID(){
	auto idx = _internal::variant_idx<T>(static_cast<ValueTypes*>(nullptr));
	return idx<std::variant_size_v<ValueTypes>?idx:std::variant_npos;
}
template<typename T> static const char* typeName() requires (typeID<T>()!=std::variant_npos){
	return _internal::name_visitor().operator()(T());
}

}

// define +/- operations for timestamp and date
/// @cond _internal
namespace std{
	template<> struct plus<isis::util::date>:binary_function<isis::util::date,isis::util::duration,isis::util::date>{
		isis::util::date operator() (const isis::util::date& x, const isis::util::duration& y) const {return x+chrono::duration_cast<chrono::days>(y);}
	};
	template<> struct minus<isis::util::date>:binary_function<isis::util::date,isis::util::duration,isis::util::date>{
		isis::util::date operator() (const isis::util::date& x, const isis::util::duration& y) const {return x-chrono::duration_cast<chrono::days>(y);}
	};
	isis::util::date &operator+=(isis::util::date &x,const isis::util::duration &y);
	isis::util::date &operator-=(isis::util::date &x,const isis::util::duration &y);
	
	template<> struct plus<isis::util::timestamp>:binary_function<isis::util::timestamp,isis::util::duration,isis::util::timestamp>{
		isis::util::timestamp operator() (const isis::util::timestamp& x, const isis::util::duration& y) const {return x+y;}
	};
	template<> struct minus<isis::util::timestamp>:binary_function<isis::util::timestamp,isis::util::duration,isis::util::timestamp>{
		isis::util::timestamp operator() (const isis::util::timestamp& x, const isis::util::duration& y) const {return x-y;}
	};

	//duration can actually be multiplied, but only with its own "ticks"
	template<> struct multiplies<isis::util::duration>:binary_function<isis::util::duration,isis::util::duration::rep,isis::util::duration>{
		isis::util::duration operator() (const isis::util::duration& x, const isis::util::duration::rep& y) const {return x*y;}
	};
	template<> struct divides<isis::util::duration>:binary_function<isis::util::duration,isis::util::duration::rep,isis::util::duration>{
		isis::util::duration operator() (const isis::util::duration& x, const isis::util::duration::rep& y) const {return x/y;}
	};

	/// streaming output for duration and timestamp
	ostream& operator<<( ostream &out, const isis::util::duration &s );
	ostream& operator<<( ostream &out, const isis::util::timestamp &s );
	ostream& operator<<( ostream &out, const isis::util::date &s );
}
/// @endcond _internal
