//
// Created by Enrico Reimer on 12.08.21.
//

#pragma once

#include "common.hpp"
#include "istring.hpp"
#include "charconv"

namespace isis::util{

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

template<typename traits>
bool stringTofloat(const std::basic_string<char, traits> &str, float& target){
	char *end;
	target= strtof(str.data(),&end);
	return str.data()+str.length() == end;
}
template<typename traits>
bool stringTofloat(const std::basic_string<char, traits> &str, double& target){
	char *end;
	target= strtof(str.data(),&end);
	return str.data()+str.length() == end;
}
template<typename traits>
bool stringTofloat(const std::basic_string<char, traits> &str, long double& target){
	char *end;
	target= strtof(str.data(),&end);
	return str.data()+str.length() == end;
}
/**
 * Try a static conversion from string to any type.
 * @param str the string to be reinterpreted as TARGET
 * @param target the reference where the converted value will be stored
 * @return true if conversion was successful, false otherwise
 */
template<typename TARGET, typename traits>
bool stringTo(const std::basic_string<char, traits> &str, TARGET& target){
	if constexpr(std::is_integral_v<TARGET> && !std::is_same_v<bool,TARGET>){
		auto [p,ec] = std::from_chars(str.data(),str.data()+str.size(),target);
		return ec == std::errc();
	} else if constexpr(std::is_floating_point_v<TARGET>){
		return stringTofloat(str,target);// @todo get rid of this once from_chars is available for float
	} else if constexpr(std::is_same_v<std::string,TARGET>){
		target=std::string(str.data(),str.length());
		return true;
	} else if constexpr(std::is_same_v<istring,TARGET>){
		target=istring(str.data(),str.length());
		return true;
	} else { //
		std::stringstream stream({str.data(),str.length()});
		stream>>target;
		bool eof = stream.eof() || stream.tellg()>=str.length();
		LOG_IF(stream.fail(),CoreDebug,error) << "Failed to read string "	<< str;
		LOG_IF(!eof,CoreDebug,warning)
		<< "Didn't use all " << str << " while converting it to " << target << " stopped at " << str.substr(stream.tellg());
		return !stream.fail() && eof;
	}
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
template<typename TARGET, typename charT=char, typename traits=std::char_traits<charT>>
std::list<TARGET> stringToList(
	const std::basic_string<charT, traits> &source,
	const std::basic_regex<charT> separator = std::basic_regex<charT>("[\\s,;]+", std::regex_constants::optimize)
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
template<typename TARGET, typename charT=char, typename traits=std::char_traits<charT>>
std::list<TARGET> stringToList(
	std::basic_string<charT, traits> source,
	const std::regex &separator,
	const std::regex prefix, const std::regex postfix
){
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
template<typename TARGET, typename charT=char, typename traits=std::char_traits<charT>>
std::list<TARGET> stringToList(const std::basic_string<charT, traits> &source, charT separator)
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

}