/*
 Copyright (C) 2010  reimer@cbs.mpg.de

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#pragma once
#include <map>
#include "common.hpp"
#include "istring.hpp"
#include <cassert>
#include <limits>

namespace isis::util
{

/**
 * Here, a Selection is one of our types (see types.hpp) and
 * meant as an enumeration of "things" described by strings,
 * e.g. properties for easy access of several properties from a PopertyMap.
 * It's using isis::util::istring, therefore
 * the options are CASE INSENSITIVE.
 */
class Selection
{
	typedef std::map<util::istring, unsigned int> MapType;
	MapType ent_map;
	MapType::mapped_type m_set = std::numeric_limits<MapType::mapped_type>::max();
	template<typename OPERATION> bool comp_op(const Selection &ref,const OPERATION &op)const{
		LOG_IF(ent_map != ref.ent_map,Debug,error)
			<< "Comparing different Selection sets "
			<< std::make_pair(getEntries(),ref.getEntries())
			<< ", result will be \"false\"";
		LOG_IF(!(m_set && ref.m_set),Debug,error) << "Comparing unset Selection, result will be \"false\"";
		return //"unset" Selections should always compare false (like NaN)
			(m_set!=std::numeric_limits<MapType::mapped_type>::max() && ref.m_set!=std::numeric_limits<MapType::mapped_type>::max())
			&& ent_map == ref.ent_map
			&& op(m_set, ref.m_set)
		;
	}
public:
	/**
	 * Default constructor.
	 * Creates a selection with the given options.
	 * \param entries comma separated list of the options as a string
	 * \param init_val the string which should be selected after initialisation (must be one from entries)
	 * \warning this is really only <b>comma</b> separated, so write "first,second,and,so,on" and not "first, second, and, so, on"
	 */
	Selection(std::initializer_list<std::string> entries, const char *init_val = "" );
	/**
	 * Default constructor.
	 * Creates a selection from a number-option map.
	 * \param map a map which maps specific numbers to options to be used
	 */
	template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
	explicit Selection( const std::map<T, std::string> &map, MapType::mapped_type init_val=std::numeric_limits<MapType::mapped_type>::max() );
	/// Fallback contructor to enable creation of empty selections
	Selection()=default;
	/**
	 * Set the selection to the given type.
	 * If the given option does not exist, a runtime error will be send and the selection won't be set.
	 * \param entry the option the selection should be set to.
	 * \returns true if the option was set, false otherwise.
	 */
	bool set( const char *entry );
	/**
	 * Set the selection to the given type.
	 * If the given option does not exist, a debug error will be send and the selection will be invalid.
	 * \param entry the option id the selection should be set to.
	 */
	void set( unsigned int entry );
	/**
	 * Set the selection to the given type.
	 * If the given option does not exist, a debug error will be send and the selection will be invalid.
	 * \param entry the option id the selection should be set to.
	 */
	bool idExists( unsigned int entry );
	/**
	 * Explicit cast to int.
	 * The numbers correspond to the order the options where given at the creation of the selection (first option -> 0, second option -> 1 ...)
	 * \returns number corresponding the currently set option or "0" if none is set
	 */
	explicit operator int()const;
	/**
	 * Implicit cast to string.
	 * \returns the currently set option or "<<NOT_SET>>" if none is set
	 */
	operator std::string()const;
	/**
	 * Implicit cast to istring.
	 * \returns the currently set option or "<<NOT_SET>>" if none is set
	 */
	operator util::istring()const;
	/**
	 * Common comparison.
	 * \returns true if both selection have the same options and are currently set to the the option. False otherwise.
	 */
	bool operator==( const Selection &ref )const;
	bool operator!=( const Selection &ref )const;
	bool operator>( const Selection &ref )const;
	bool operator<( const Selection &ref )const;
	/**
	 * String comparison.
	 * \returns true if the currently set option is non-case-sensitive equal to the given string. False otherwise.
	 */
	bool operator==( const std::string_view &ref )const;
	bool operator!=( const std::string_view &ref )const;

	/// \returns a list of all options
	[[nodiscard]] std::list<util::istring> getEntries()const;
};

template<typename T, std::enable_if_t<std::is_integral_v<T>, int>>
Selection::Selection( const std::map< T, std::string >& map, MapType::mapped_type init_val):m_set(init_val)
{
	for( const auto &m : map) {
		const MapType::value_type pair(
			util::istring( m.second.begin(), m.second.end() ),
			m.first
		);
		assert( pair.second != std::numeric_limits<MapType::mapped_type>::max() ); // MAX_INT is reserved for <<NOT_SET>>

		if( !ent_map.insert( pair ).second ) {
			LOG( Debug, error ) << "Entry " << util::MSubject( pair ) << " could not be inserted";
		}
	}
}

}
/// @cond _internal
namespace std
{
/// Streaming output for selections.
template<typename charT, typename traits>
basic_ostream<charT, traits> &operator<<( basic_ostream<charT, traits> &out, const isis::util::Selection &s )
{
	return out << ( std::string )s;
}
}
/// @endcond _internal
