#ifndef TYPES_HPP
#define TYPES_HPP

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


#endif // TYPES_HPP
