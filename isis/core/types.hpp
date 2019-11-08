#ifndef VALUETYPES_HPP
#define VALUETYPES_HPP

#include <variant>
#include <complex>
#include "color.hpp"
#include "vector.hpp"
#include "selection.hpp"
#include <iomanip>

namespace std{namespace chrono{
typedef duration<int32_t,ratio<int(3600*24)> > days;  
}}

namespace isis::util{

typedef std::list<int32_t> ilist;
typedef std::list<double> dlist;
typedef std::list<std::string> slist;
typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds> timestamp;
typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::days> date;
typedef timestamp::duration duration; // @todo float duration might be nice

typedef std::variant<
  bool
, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
, float, double
, color24, color48
, fvector3, dvector3, ivector3
, fvector4, dvector4, ivector4
, ilist, dlist, slist
, std::string, isis::util::Selection
, std::complex<float>, std::complex<double>
, date, timestamp, duration
> ValueTypes;

namespace _internal{
struct name_visitor{
    template<typename T> std::string operator()(const T &)const{
        return std::string("unnamed_")+typeid(T).name();
    }
};
template<bool ENABLED> struct ordered{
	static const bool lt=ENABLED,gt=ENABLED;
};
template<bool ENABLED> struct multiplicative{
	static const bool mult=ENABLED,div=ENABLED;
};
template<bool ENABLED> struct additive{
	static const bool plus=ENABLED,minus=ENABLED,negate=ENABLED;
};
}

/**
 * resolves to true if type is known, false if not
 */
template< typename T > constexpr bool knownType(){ //@todo can we get rid of this?
#warning implement me
	return true;
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


/**
 * Get a std::map mapping type IDs to type names.
 * \note the list is generated at runtime, so doing this excessively will be expensive.
 * \param withValues include util::Value-s in the map
 * \param withValueArrays include data::ValueArray-s in the map
 * \returns a map, mapping util::Value::staticID() and data::ValueArray::staticID() to util::Value::staticName and data::ValueArray::staticName
 */
std::map<unsigned short, std::string> getTypeMap( bool withValues = true, bool withValueArrays = true );

/**
 * Inverse of getTypeMap.
 * \note the list is generated at runtime, so doing this excessively will be expensive.
 * \param withValues include util::Value-s in the map
 * \param withValueArrays include data::ValueArray-s in the map
 * \returns a map, mapping util::Value::staticName and data::ValueArray::staticName to util::Value::staticID() and data::ValueArray::staticID()
 */
std::map< std::string, unsigned short> getTransposedTypeMap( bool withValues = true, bool withValueArrays = true );

}

// define +/- operations for timestamp and date
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
			out<<std::put_time(std::localtime(&tme), "%X"); // otherwise write just the time 
		}
		// and maybe with milliseconds
		
		chrono::milliseconds msec = s.time_since_epoch()-sec;
		assert(msec.count()<1000);
		if(msec.count()){
			if(msec.count()<0)
				msec+=chrono::seconds(1);
			// we dont want to mess with out, so we don't use stream formatting
			char buff[5];
			snprintf(buff,5,"%3lld",msec.count());
			out << "+" << buff << "ms"; 
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

#endif // VALUETYPES_HPP
