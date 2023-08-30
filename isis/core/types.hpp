#pragma once

#include <variant>
#include <complex>
#include "color.hpp"
#include "vector.hpp"
#include "selection.hpp"
#include <iomanip>

#if __GNUC__ < 10
namespace std::chrono{
typedef duration<int32_t,ratio<int(3600*24)> > days;
}
#endif
namespace isis::util{

typedef std::list<int32_t> ilist;
typedef std::list<double> dlist;
typedef std::list<std::string> slist;
typedef std::chrono::system_clock::duration duration; // @todo float duration might be nice
typedef std::chrono::sys_time<duration> timestamp;
typedef std::chrono::sys_days date;


/// @cond _internal
namespace _internal{
struct name_visitor{
	//linking will fail if a setName( ... ); is missing in types.cpp
	template<typename T> const char* operator()(const T &)const;
};

template<typename Search, typename _First, typename... _Rest>
constexpr size_t variant_idx(std::variant<_First, _Rest...>*)
{
	if(std::is_same_v<std::remove_cvref_t<Search>,_First>)//current variant is a hit, terminate the recursion without adding
		return 0;
	else if constexpr(sizeof...(_Rest))  //try the next variant if there is one, add 1
		return variant_idx<Search>(static_cast<std::variant<_Rest...>*>(nullptr))+1;
	else
		return 1;//we ran out of variants terminate recursion adding 1 to the last index
}

}
/// @encond _internal

/**
 * Get a std::map mapping type IDs to type names.
 * \note the list is generated at runtime, so doing this excessively will be expensive.
 * \param withValues include util::Value-s in the map
 * \param withValueArrays include data::ValueArray-s in the map
 * \returns a map, mapping util::Value::staticID() and data::ValueArray::staticID() to util::Value::staticName and data::ValueArray::staticName
 */
const std::map<size_t, std::string> &getTypeMap(bool arrayTypesOnly=false);

/**
 * Inverse of getTypeMap.
 * \note the list is generated at runtime, so doing this excessively will be expensive.
 * \param withValues include util::Value-s in the map
 * \param withValueArrays include data::ValueArray-s in the map
 * \returns a map, mapping util::Value::staticName and data::ValueArray::staticName to util::Value::staticID() and data::ValueArray::staticID()
 */
std::map< std::string, unsigned short> getTransposedTypeMap( bool withValues = true, bool withValueArrays = true );

}



