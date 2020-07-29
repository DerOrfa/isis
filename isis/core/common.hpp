//
// C++ Interface: common
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#pragma once

#ifdef _MSC_VER
#define NOMINMAX 1
#endif

#include <utility>
#include <ostream>
#include <map>
#include <string>
#include <sstream>
#include <regex>
#include <set>
#include <cmath>
#include "log.hpp"
#include "log_modules.hpp"
#include "../config.hpp"
#include <charconv>
#include <complex>

#ifdef WIN32
#include <Windows.h>
#endif

namespace isis
{
namespace util
{

/**
* Continously searches in a sorted list using the given less-than comparison.
* It starts at current and increments it until the referenced value is not less than the compare-value anymore.
* Than it returns.
* \param current the current-position-iterator for the sorted list.
* This value is changed directly, so after the function returns is references the first entry of the list
* which does not compare less than compare or, if such a value does not exist in the list, it will be equal to end.
* \param end the end of the list
* \param compare the compare-value
* \param compOp the comparison functor. It must provide "bool operator()(T,T)".
* \returns true if the value current currently refers to is equal to compare
*/
template<typename ForwardIterator, typename T, typename CMP>
bool
continousFind(ForwardIterator &current, const ForwardIterator end, const T &compare, const CMP &compOp)
{
	//find the first iterator which does not compare less
	current = std::lower_bound(current, end, compare, compOp);

	if (current == end //if we're at the end
		|| compOp(compare, *current) //or compare less than that iterator (eg. iterator greater than compare)
		)
		return false;//we didn't find a match
	else
		return true;//not(current <> compare) makes compare == current
}

std::string getLastSystemError();
std::filesystem::path getRootPath(std::list<std::filesystem::path> sources, bool sorted = false);

std::filesystem::path pathReduce(std::set<std::filesystem::path> sources);

/**
Write a list of elements to a std::basic_ostream
\param start starting iterator of input
\param end end iterator of input
\param o the output stream to write into
\param delim delimiter used to seperate the elements (default: " ")
\param prefix will be send to the stream as start (default: "")
\param suffix will be send to the stream at the end (default: "")
*/
template<class InputIterator, typename _CharT, typename _Traits>
std::basic_ostream<_CharT, _Traits> &
listToOStream(InputIterator start, InputIterator end,
			  std::basic_ostream<_CharT, _Traits> &o,
			  const _CharT *delim = ",",
			  const _CharT *prefix = "{", const _CharT *suffix = "}")
{
	o << prefix;

	if (start != end)
		o << *(start++);

	for (InputIterator i = start; i != end; i++)
		o << delim << *i;

	o << suffix;
	return o;
}

// specialization to print char-list as number lists, not strings
//@todo check if this works for VC
template<typename _CharT, typename _Traits>
std::basic_ostream<_CharT, _Traits> &
listToOStream(const unsigned char *start, const unsigned char *end,
			  std::basic_ostream<_CharT, _Traits> &o,
			  const std::string delim = ",",
			  const std::string prefix = "{", const std::string suffix = "}")
{
	o << prefix;

	if (start != end) {
		o << (unsigned short) *start;
		start++;
	}

	for (const unsigned char *i = start; i != end; i++)
		o << delim << (unsigned short) *i;

	o << suffix;
	return o;
}

/// use listToOStream to create a string from a list
template<class TARGET=std::string, class InputIterator>
TARGET listToString(
	InputIterator start, InputIterator end,
	const typename TARGET::value_type *delim = ",",
	const typename TARGET::value_type *prefix = "{", const typename TARGET::value_type *suffix = "}"
)
{
	std::basic_ostringstream<typename TARGET::value_type, typename TARGET::traits_type> ret;
	listToOStream(start, end, ret, delim, prefix, suffix);
	return ret.str();
}

/**
 * Try a static conversion from string to any type.
 * @param str the string to be reinterpreted as TARGET
 * @param target the refernce where the converted value will be stored
 * @return true if conversion was successful, false otherweise
 */
template<typename TARGET, typename traits>
bool stringTo(const std::basic_string<char, traits> &str, TARGET& target){
	std::basic_stringstream<char, traits> stream(str);
	stream>>target;
	LOG_IF(!stream.eof(),CoreDebug,warning)
		<< "There are some characters left after parsing "
		<< util::MSubject(str) << "(" << util::MSubject(stream.str()) << ")";
	return stream.eof();
}
/**
 * Specialisation of stringTo for string to string conversion.
 * @param str the string to be reinterpreted as TARGET
 * @param target the refernce where the converted value will be stored
 * @return true if conversion was successful, false otherweise
 */
template<typename traits1,typename traits2>
bool stringTo(const std::basic_string<char, traits1> &str, std::basic_string<char, traits2>& target){
	target=str.c_str();
	return true;
}
/**
 * Specialisations of stringTo for string to floating_point conversion.
 * @param str the string to be reinterpreted as TARGET
 * @param target the refernce where the converted value will be stored
 * @return true if conversion was successful, false otherweise
 */
template<typename traits>
bool stringTo(const std::basic_string<char, traits> &str, float& target){
	target=std::stof(str.c_str());
	return true;
}
template<typename traits>
bool stringTo(const std::basic_string<char, traits> &str, double& target){
	target=std::stod(str.c_str());
	return true;
}
template<typename traits>
bool stringTo(const std::basic_string<char, traits> &str, long double& target){
	target=std::stold(str.c_str());
	return true;
}
// @todo be careful stringTo is used in the automatic conversion, so don't use the automatic conversion to implement stringTo

/**
 * Simple tokenizer (regexp version).
 * Splits source into tokens and tries to lexically cast them to TARGET.
 * If that fails, errors are sent and the no element will be added to the list.
 * Before the string is split up leading and rear separators will be cut.
 * \param source the source string to be split up
 * \param separator regular expression to delimit the tokens (defaults to [^\\s,;])
 * \returns a list of the casted tokens
 */
template<typename TARGET, typename traits>
std::list<TARGET> stringToList(
	const std::basic_string<char, traits> &source,
	const std::regex separator = std::regex("[\\s,;]+", std::regex_constants::optimize)
){
	std::list<TARGET> ret;
	using tokenizer_it = std::regex_token_iterator<const char*>;
	tokenizer_it i(source.data(), source.data()+source.size(), separator, -1);

	while (i != tokenizer_it()) {
		if(!stringTo((i++)->str(), ret.emplace_back()))
			ret.pop_back();//remove element again
	}
	return ret;
}

template<typename TARGET>
std::list<TARGET> stringToList(
	const char source[],
	const std::regex separator = std::regex("[\\s,;]+", std::regex_constants::optimize))
{
	return stringToList<TARGET>(std::string(source), separator);
}
/**
 * Generic tokenizer.
 * Splits source into tokens and tries to lexically cast them to TARGET.
 * If that fails, errors are sent and the no element will be added to the list.
 * \param source the source string to be split up
 * \param separator string to delimit the tokens
 * \param prefix regular expression for text to be removed from the string before it is split up
 * ("^" is recommended to be there)
 * \param postfix regular expression for text to be removed from the string before it is split up
 * ("$" is recommended to be there)
 * \returns a list of the casted tokens
 */
template<typename TARGET, typename charT, typename traits>
std::list<TARGET> stringToList(
	std::basic_string<charT, traits> source, const std::regex &separator,
	const std::regex prefix, const std::regex postfix)
{
	std::list<TARGET> ret;
	const std::string empty;
	source = std::regex_replace(source, prefix, empty);
	source = std::regex_replace(source, postfix, empty);

	return stringToList<TARGET>(source, separator);
}

/**
 * Very Simple tokenizer.
 * Splits source into tokens and tries to lexically cast them to TARGET.
 * If that fails, errors are sent and the no element will be added to the list.
 * Leading and trailing seperators are ignored.
 *
 * \param source the source string to be split up
 * \param separator character to delimit the tokens
 * \returns a list of the casted tokens
 */
//@todo test
template<typename TARGET, typename charT, typename traits>
std::list<TARGET>
stringToList(const std::basic_string<charT, traits> &source, charT separator)
{
	std::list<TARGET> ret;

	for (
		size_t next = source.find_first_not_of(separator);
		next != std::string::npos;
		next = source.find_first_not_of(separator, next)
	) {
		const size_t start = next;
		next = source.find(separator, start);
		if(!stringTo(source.substr(start, next - start),ret.emplace_back()))
			ret.pop_back();//remove element again
	}

	return ret;
}

/**
 * Fuzzy comparison between floating point values.
 * - Will return true if and only if the difference between two values is "small" compared to their magnitude.
 * - Will raise a compiler error when not used with floating point types.
 * @param a first value to compare with
 * @param b second value to compare with
 * @param scale scaling factor to determine which multiplies of the types floating point resolution (epsilon) should be considered equal
 * Eg. "1" means any difference less than the epsilon of the used floating point type will be considered equal.
 * If any of the values is greater than "1" the "allowed" difference will be bigger.
 * \returns \f[ |a-b| <= \varepsilon_T * \lceil |a|,|b|,|thresh| \rceil \f].
 */
template<typename T>
bool fuzzyEqual(T a, T b, unsigned short scale = 10)
{
	static_assert(std::is_floating_point<T>::value, "must be called with floating point");

	static const T
		epsilon = std::numeric_limits<T>::epsilon(); // get the distance between 1 and the next representable value
	T bigger, smaller;

	a = std::abs(a);
	b = std::abs(b);

	if (a < b) {
		bigger = b;
		smaller = a;
	}
	else {
		smaller = b;
		bigger = a;
	}

	if (smaller == 0)
		return bigger < (epsilon * scale);

	const T factor = 1 / smaller; // scale smaller to that value
	return (bigger * factor)
		<= (1 + epsilon * scale); //scaled bigger should be between 1 and the next representable value
}

typedef CoreDebug Debug;

typedef CoreLog Runtime;

/**
 * Set logging level for the namespace util.
 * This logging level will be used for every LOG(Debug,...) and LOG(Runtime,...) within the util namespace.
 * This is affected by by the _ENABLE_LOG and _ENABLE_DEBUG settings of the current compile and won't have an
 * effect on the Debug or Runtime logging if the corresponding define is set to "0".
 * So if you compile with "-D_ENABLE_DEBUG=0" against a library which (for example) was comiled with "-D_ENABLE_DEBUG=1",
 * you won't be able to change the logging level of the debug messages of these library.
 */
template<typename HANDLE>
void enableLog(LogLevel level)
{
	ENABLE_LOG(CoreLog, HANDLE, level);
	ENABLE_LOG(CoreDebug, HANDLE, level);
}

}//util
namespace image_io
{
typedef ImageIoLog Runtime;

typedef ImageIoDebug Debug;

template<typename HANDLE>
void enableLog(LogLevel level)
{
	ENABLE_LOG(Runtime, HANDLE, level);
	ENABLE_LOG(Debug, HANDLE, level);
}
} //namespace image_io

namespace data
{

namespace _internal
{
}

typedef DataLog Runtime;

typedef DataDebug Debug;

enum dimensions
{
	rowDim = 0, columnDim, sliceDim, timeDim
};
enum scannerAxis
{
	x = 0, y, z, t
};

/**
 * Set logging level for the namespace data.
 * This logging level will be used for every LOG(Debug,...) and LOG(Runtime,...) within the data namespace.
 * This is affected by by the _ENABLE_LOG and _ENABLE_DEBUG settings of the current compile and won't have an
 * effect on the Debug or Runtime logging if the corresponding define is set to "0".
 * So if you compile with "-D_ENABLE_DEBUG=0" against a library which (for example) was comiled with "-D_ENABLE_DEBUG=1",
 * you won't be able to change the logging level of the debug messages of these library.
 */
template<typename HANDLE>
void enableLog(LogLevel level)
{
	ENABLE_LOG(Runtime, HANDLE, level);
	ENABLE_LOG(Debug, HANDLE, level);
}
}

/**
 * Set logging level for the namespaces util,data and image_io.
 * This logging level will be used for every LOG(Debug,...) and LOG(Runtime,...) within the image_io namespace.
 * This is affected by by the _ENABLE_LOG and _ENABLE_DEBUG settings of the current compile and won't have an
 * effect on the Debug or Runtime logging if the corresponding define is set to "0".
 * So if you compile with "-D_ENABLE_DEBUG=0" against a library which (for example) was comiled with "-D_ENABLE_DEBUG=1",
 * you won't be able to change the logging level of the debug messages of these library.
 */
template<typename HANDLE>
void enableLogGlobal(LogLevel level)
{
	ENABLE_LOG(CoreLog, HANDLE, level);
	ENABLE_LOG(CoreDebug, HANDLE, level);
	ENABLE_LOG(ImageIoLog, HANDLE, level);
	ENABLE_LOG(ImageIoDebug, HANDLE, level);
	ENABLE_LOG(DataLog, HANDLE, level);
	ENABLE_LOG(DataDebug, HANDLE, level);
}
}//isis

namespace std
{
/// Streaming output for std::pair
template<typename charT, typename _FIRST, typename _SECOND > basic_ostream<charT, std::char_traits<charT> >&
operator<<( basic_ostream<charT, std::char_traits<charT> > &out, const pair<_FIRST, _SECOND> &s )
{
	return out << s.first << ":" << s.second;
}

/// Streaming output for std::map
template<typename charT, typename _Key, typename _Tp, typename _Compare, typename _Alloc >
basic_ostream<charT, std::char_traits<charT> >& operator<<( basic_ostream<charT, std::char_traits<charT> > &out, const map<_Key, _Tp, _Compare, _Alloc>& s )
{
	isis::util::listToOStream( s.begin(), s.end(), out, "\n", "", "" );
	return out;
}

/// Formatted streaming output for std::map\<string,...\>
template<typename charT, typename _Tp, typename _Compare, typename _Alloc >
basic_ostream<charT, std::char_traits<charT> >& operator<<( basic_ostream<charT, std::char_traits<charT> > &out, const map<std::string, _Tp, _Compare, _Alloc>& s )
{
	size_t key_len = 0;
	typedef typename map<std::string, _Tp, _Compare, _Alloc>::const_iterator m_iterator;

	for ( m_iterator i = s.begin(); i != s.end(); i++ )
		if ( key_len < i->first.length() )
			key_len = i->first.length();

	for ( m_iterator i = s.begin(); i != s.end(); ) {
		out << make_pair( i->first + std::string( key_len - i->first.length(), ' ' ), i->second );

		if ( ++i != s.end() )
			out << std::endl;
	}

	return out;
}

///Streaming output for std::list
template<typename charT, typename _Tp, typename _Alloc >
basic_ostream<charT, std::char_traits<charT> >& operator<<( basic_ostream<charT, std::char_traits<charT> > &out, const list<_Tp, _Alloc>& s )
{
	isis::util::listToOStream( s.begin(), s.end(), out );
	return out;
}
///Streaming output for std::set
template<typename charT, typename _Tp, typename _Alloc >
basic_ostream<charT, std::char_traits<charT> >& operator<<( basic_ostream<charT, std::char_traits<charT> > &out, const set<_Tp, _Alloc>& s )
{
	isis::util::listToOStream( s.begin(), s.end(), out );
	return out;
}

}
