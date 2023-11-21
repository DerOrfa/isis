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
	isis::util::date &operator+=(isis::util::date &x,const isis::util::duration &y);
	isis::util::date &operator-=(isis::util::date &x,const isis::util::duration &y);

	/// streaming output for duration and timestamp
	ostream& operator<<( ostream &out, const isis::util::duration &s );
	ostream& operator<<( ostream &out, const isis::util::timestamp &s );
	ostream& operator<<( ostream &out, const isis::util::date &s );
}
/// @endcond _internal
