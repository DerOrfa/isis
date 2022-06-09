#pragma once

#include <variant>
#include <complex>
#include "color.hpp"
#include "vector.hpp"
#include "selection.hpp"
#include <iomanip>

namespace isis::util{

typedef std::list<int32_t> ilist;
typedef std::list<double> dlist;
typedef std::list<std::string> slist;
typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::milliseconds> timestamp;
typedef std::chrono::time_point<std::chrono::system_clock,std::chrono::days> date;
typedef timestamp::duration duration; // @todo float duration might be nice


/// @cond _internal
namespace _internal{
struct name_visitor{
	//linking will fail if a setName( ... ); is missing in types.cpp
	template<typename T> const char* operator()(const T &)const;
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

template<typename VariantType, typename T, std::size_t index = 0> constexpr std::size_t variant_index() {
    if constexpr (index >= std::variant_size_v<VariantType>) {
        return std::variant_npos;
    } else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>) {
        return index;
    } else {
        return variant_index<VariantType, T, index + 1>();
    }
}

}
/// @encond _internal

template<typename T> static const char* typeName(){
	return _internal::name_visitor().operator()(T());
}

/**
 * Get a std::map mapping type IDs to type names.
 * \note the list is generated at runtime, so doing this excessively will be expensive.
 * \param withValues include util::Value-s in the map
 * \param withValueArrays include data::ValueArray-s in the map
 * \returns a map, mapping util::Value::staticID() and data::ValueArray::staticID() to util::Value::staticName and data::ValueArray::staticName
 */
std::map<size_t, std::string> getTypeMap( bool arrayTypesOnly = false );

/**
 * Inverse of getTypeMap.
 * \note the list is generated at runtime, so doing this excessively will be expensive.
 * \param withValues include util::Value-s in the map
 * \param withValueArrays include data::ValueArray-s in the map
 * \returns a map, mapping util::Value::staticName and data::ValueArray::staticName to util::Value::staticID() and data::ValueArray::staticID()
 */
std::map< std::string, unsigned short> getTransposedTypeMap( bool withValues = true, bool withValueArrays = true );

}



