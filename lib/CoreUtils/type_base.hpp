//
// C++ Interface: type_base
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef ISISTYPE_BASE_HPP
#define ISISTYPE_BASE_HPP

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include "types.hpp"
#include "type_converter.hpp"
#include "generic_type.hpp"
#include "common.hpp"


/*! \addtogroup util
*  Additional documentation for group `mygrp'
*  @{
*/
namespace isis
{
namespace util
{

/// @cond _hidden
namespace _internal
{
template<typename TYPE, typename T> TYPE __cast_to( Type<TYPE> *dest, const T &value )
{
	return boost::lexical_cast<TYPE>( value );
}
template<typename TYPE> TYPE __cast_to( Type<TYPE> *dest, const TYPE &value )
{
	return value;
}
/// @endcond

/// @cond _internal

class TypeBase : public GenericType
{
	static const TypeConverterMap &converters();
	friend class TypeReference<TypeBase>;
protected:
	/**
	* Create a copy of this.
	* Creates a new Type/TypePtr an stores a copy of its value there.
	* Makes TypeBase-pointers copyable without knowing their type.
	* \returns a TypeBase-pointer to a newly created Type/TypePtr.
	*/
	virtual TypeBase *clone()const = 0;
public:
	typedef TypeReference<TypeBase> Reference;
	typedef TypeConverterMap::mapped_type::mapped_type Converter;

	const Converter &getConverterTo( unsigned short id )const;
	/**
	 * Convert the content of one Type to another.
	 * This will use the automatic conversion system to transform the value one Type-Object into another.
	 * The types of both objects can be unknown.
	 * \param from the Type-object containing the value which should be converted
	 * \param to the Type-object which will contain the converted value if conversion was successfull
	 * \returns false if the conversion failed for any reason, true otherwise
	 */
	static bool convert( const TypeBase &from, TypeBase &to );
	/**
	* Interpret the value as value of any (other) type.
	* This is a runtime-based cast via automatic conversion.
	* \code
	* TypeBase *mephisto=new Type<std::string>("666");
	* int devil=mephisto->as<int>();
	* \endcode
	* If you know the type of source and destination at compile time you should use Type\<DEST_TYPE\>((SOURCE_TYPE)src).
	* \code
	* Type<std::string> mephisto("666");
	* Type<int> devil((std::string)devil);
	* \endcode
	* \return value of any requested type parsed from toString(false).
	*/
	template<class T> T as()const {
		if ( typeID() == Type<T>::staticID ) {
			LOG( Debug, verbose_info )
					<< "Doing reinterpret_cast instead of useless conversion from " << toString( true )
					<< " to " << Type<T>::staticName();
			return *reinterpret_cast<const Type<T>*>( this );
		} else {
			Type<T> ret;

			if ( ! convert( *this, ret ) ) {
				LOG( Debug, error )
						<< "Interpretation of " << toString( true ) << " as " << Type<T>::staticName()
						<< " failed. Returning " << Type<T>().toString() << ".";
				return T();
			} else
				return ret;
		}
	}

	/**
	 * Dynamically cast the TypeBase up to its actual Type\<T\>. Constant version.
	 * Will throw std::bad_cast if T is not the actual type.
	 * Will send an error if T is not the actual type and _ENABLE_CORE_LOG is true.
	 * \returns a constant reference of the stored value.
	 */
	template<typename T> const Type<T>& cast_to_Type() const {
		check_type<T>();
		return m_cast_to<Type<T> >();
	}
	/**
	 * Dynamically cast the TypeBase up to its actual Type\<T\>. Referenced version.
	 * Will throw std::bad_cast if T is not the actual type.
	 * Will send an error if T is not the actual type and _ENABLE_CORE_LOG is true.
	 * \returns a reference of the stored value.
	 */
	template<typename T> Type<T>& cast_to_Type() {
		check_type<T>();
		return m_cast_to<Type<T> >();
	}
	virtual bool operator==( const TypeBase &second )const = 0;

	virtual ~TypeBase();

	virtual bool gt( const _internal::TypeBase &ref )const = 0;
	virtual bool lt( const _internal::TypeBase &ref )const = 0;
	virtual bool eq( const _internal::TypeBase &ref )const = 0;
};


}
/// @endcond
}
}

namespace std
{
/// Streaming output for Type - classes
template<typename charT, typename traits> basic_ostream<charT, traits>&
operator<<( basic_ostream<charT, traits> &out, const isis::util::_internal::GenericType &s )
{
	return out << s.toString();
}
/// /// Streaming output for Type referencing classes
template<typename charT, typename traits, typename TYPE_TYPE> basic_ostream<charT, traits>&
operator<<( basic_ostream<charT, traits> &out, const isis::util::_internal::TypeReference<TYPE_TYPE> &s )
{
	return out << s.toString( true );
}
}

/// }@

#endif //ISISTYPE_BASE_HPP
