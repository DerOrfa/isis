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

#include "property.hpp"
#include <string>
#include <map>
#include <ostream>

namespace isis::util
{
/**
 * Container class for programm parameters (commandline parameters) given to applications derived from isis.
 * It is derived from PropertyValue, and thus can store any known type.
 * Additionally, it holds a description of the parameter and
 * parses the given strings to cast them to correct internal parameter types.
 */
class ProgParameter: public PropertyValue
{
	std::string m_description;
	bool m_hidden=false, m_parsed=false;
public:
	/**
	 * Default constructor.
	 * This creates an empty/typeless parameter container.
	 * Note that empty containers cannot parse a value because they dont have a type they could parse into.
	 * Thus parameters created using this must be set to any type/value before parse() is called.
	 */
	ProgParameter();
	/**
	 * Create a programm parameter using an initial value/type.
	 * \param ref the intial value/type the programm parameter should get
	 * (The value is used as default value if the parameter never gets to parse any other value)
	 * \param is_needed flag if parameter is a needed one (default: true)
	 */
	template<KnownValueType T>
	ProgParameter( const T &ref, bool is_needed = true ): PropertyValue( ref, is_needed ), m_hidden( false ), m_parsed( false ) {}
	/**
	 * Put the given value into this parameter.
	 * The parsing is done by automatic (!) type-conversion from std::string to the type of the parameter.
	 * Parameters of type bool will be set true in any case.
	 * Important: you cannot parse into empty parameters  because they dont have a type.
	 * \param props the value as string written on commandline to be put into this parameter
	 * \return true if parsing was successful, false otherwise
	 */
	bool parse( const std::string &prop );
	bool parse_list( const util::slist &props_list );
	
	/// \return the description string
	[[nodiscard]] const std::string &description()const;
	/* set the description string
	*  \param desc description to be set */
	void setDescription( const std::string &desc );
	/**
	 * Implicit cast to Value T.
	 * If the parameter does not contain T, a runtime error will be raised
	 */
	template<typename T> operator const T()const {
		LOG_IF( isEmpty(), isis::CoreDebug, isis::error ) << "Program parameters must not be empty. Please set it to any value.";
		return std::get<T>(front());
	}

	explicit operator bool()const;

	/// \returns true, if the parameter was ever successfully parsed
	[[nodiscard]] bool isParsed()const;

	///returns true for hidden parameters, false otherwise
	[[nodiscard]] bool isHidden()const;

	///\copydoc isHidden
	bool &hidden();

	friend std::ostream &operator<<(std::ostream &os, const ProgParameter &parameter);
};

/**
 * A Map of all current Program Parameters when reading from the commandline.
 * Handles instances of ProgParameter for every expected program parameter and sets them by reading an argc/argv pair.
 */
class ParameterMap: public std::map<std::string, ProgParameter>
{
	bool parsed;
public:
	ProgParameter operator[]( const std::string key )const;
	ProgParameter &operator[]( const std::string key );
	/*
	 * Default constructor to create an empty parameter map
	 */
	ParameterMap();
	/**
	 * Reads and tokenizes argv, detecs the parameter names (names are preceded by a "-") and sends the values
	 * (everything between one parameter name and the other) to the corresponding ProgParameter-objects to be parsed.
	 * \param argc the names of the program parameters
	 * \param argv the values of the program parameters
	 * \returns true if succesfully converted all parameters to correct internal types, false otherwise
	 */
	bool parse( int argc, char **argv );

	/// \returns true, if ALL needed parameter were correctly parsed, false otherwise
	[[nodiscard]] bool isComplete()const;
};

}


