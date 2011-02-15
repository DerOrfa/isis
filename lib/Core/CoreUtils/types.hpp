#ifndef TYPES_HPP_INCLUDED
#define TYPES_HPP_INCLUDED

#include <boost/mpl/vector/vector30.hpp>
#include <boost/mpl/distance.hpp>
#include <boost/mpl/plus.hpp>

#include <boost/mpl/find.hpp>
#include <boost/mpl/contains.hpp>

#ifndef _MSC_VER
#include <stdint.h>
#endif

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "vector.hpp"
#include "color.hpp"
#include "selection.hpp"


namespace isis
{
namespace util
{

typedef std::list<int32_t> ilist;
typedef std::list<double> dlist;
typedef std::list<std::string> slist;

/// @cond _internal
namespace _internal
{
using namespace boost::mpl;

/// the supported types as mpl-vector
typedef vector23 < //increase this if a type is added (if >30 consider including vector40 above)
bool
, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
, float, double
, color24, color48
, fvector4, dvector4, ivector4
, ilist, dlist, slist
, std::string, isis::util::Selection
, boost::posix_time::ptime, boost::gregorian::date
> types;

/**
 * Templated pseudo struct to generate the ID of a supported type.
 * The ID is stored in TypeID\<T\>::value.
 * The ID is the position of the type in the mpl::vector types, starting with 1 (so there is no id==0)
 * This is a compile-time-constant, so it can be used as a template parameter and has no impact at the runtime.
 */
template<class T> struct TypeID {
	typedef plus< int_<1>, typename distance<begin<types>::type, typename find<types, T>::type >::type > type;
	static const unsigned short value = type::value;
};
}
/// @endcond
/**
 * Templated pseudo struct to check for availability of a type at compile time.
 * Instanciating this with any datatype (eg: check_type\<short\>() ) will cause the
 * compiler to raise an error if this datatype is not in the list of the supported types.
 */
template< typename T > struct check_type {
	BOOST_MPL_ASSERT_MSG(
		( boost::mpl::contains<_internal::types, T>::value )
		, TYPE_IS_NOT_KNOWN
		, ( T )
	);
};

std::map<unsigned short, std::string> getTypeMap( bool withTypes = true, bool withTypePtrs = true );

std::map< std::string, unsigned short> getTransposedTypeMap( bool withTypes = true, bool withTypePtrs = true );
}
}

#endif //TYPES_HPP_INCLUDED
