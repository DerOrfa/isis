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
	return _internal::variant_index<ValueTypes,std::remove_cv_t<T>>();
}
/**
 * Check if T is a known type.
 * @tparam T the type to be checked
 * @return true if T is a known type
 * @return false otherwise
 */
template<typename T> static constexpr bool knownType(){
	const auto id=typeID<T>();
	return id!=std::variant_npos;
}

/**
 * Templated pseudo struct to check if a type supports an operation at compile time.
 * The compile-time flags are:
 * - \c \b lt can compare less-than
 * - \c \b gt can compare greater-than
 * - \c \b mult multiplication is applicable
 * - \c \b div division is applicable
 * - \c \b plus addition is applicable
 * - \c \b minus substraction is applicable
 * - \c \b negate negation is applicable
 */
template<typename T> struct has_op:
	_internal::ordered<knownType<T>()>,
	_internal::additive<knownType<T>()>,
	_internal::multiplicative<knownType<T>()>
	{};

/// @cond _internal
template<> struct has_op<Selection>:_internal::ordered<true>,_internal::additive<false>,_internal::multiplicative<false>{};
template<> struct has_op<std::string>:_internal::ordered<true>,_internal::multiplicative<false>{static const bool plus=true,minus=false;};

// cannot multiply time
template<> struct has_op<date>:_internal::ordered<true>,_internal::additive<true>,_internal::multiplicative<false>{};
template<> struct has_op<timestamp>:_internal::ordered<true>,_internal::additive<true>,_internal::multiplicative<false>{};


// multidim is not ordered
template<typename T> struct has_op<std::list<T> >:_internal::ordered<true>,_internal::additive<false>,_internal::multiplicative<false>{};
template<typename T> struct has_op<color<T> >:_internal::ordered<false>,_internal::additive<false>,_internal::multiplicative<false>{};
template<typename T> struct has_op<std::complex<T> >:_internal::ordered<false>,_internal::additive<true>,_internal::multiplicative<true>{};
/// @endcond
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
	template<typename charT, typename traits>
	basic_ostream<charT, traits>& operator<<( basic_ostream<charT, traits> &out, const isis::util::duration &s )
	{
		return out<<s.count()<<"ms";
	}
	template<typename charT, typename traits>
	basic_ostream<charT, traits>& operator<<( basic_ostream<charT, traits> &out, const isis::util::timestamp &s )
	{
		const chrono::seconds sec=std::chrono::duration_cast<chrono::seconds>(s.time_since_epoch());
		const time_t tme(sec.count());
		if(s>=(isis::util::timestamp()+std::chrono::hours(24))) // if we have a real timepoint (not just time)
			out<<std::put_time(std::localtime(&tme), "%c"); // write time and date
		else {
			out<<std::put_time(std::localtime(&tme), "%X"); // otherwise, write just the time
		}
		// and maybe with milliseconds
		
		chrono::milliseconds msec = s.time_since_epoch()-sec;
		assert(msec.count()<1000);
		if(msec.count()){
			if(msec.count()<0)
				msec+=chrono::seconds(1);
			out << "+" << std::to_string(msec.count()) << "ms";
		}
		return out;
	}
	template<typename charT, typename traits>
	basic_ostream<charT, traits>& operator<<( basic_ostream<charT, traits> &out, const isis::util::date &s )
	{
		const time_t tme(chrono::duration_cast<chrono::seconds>(s.time_since_epoch()).count());
		return out<<std::put_time(std::localtime(&tme), "%x"); 
	}
}
/// @endcond _internal
