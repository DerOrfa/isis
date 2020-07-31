#pragma once

#include <string>
#include <locale>
#include <list>

namespace isis::util
{
/// @cond _internal
namespace _internal
{
struct ichar_traits: public std::char_traits<char> {
	static const std::locale loc;
	static bool eq ( const char_type &c1, const char_type &c2 );
	static bool lt ( const char_type &c1, const char_type &c2 );
	static int compare ( const char_type *s1, const char_type *s2, std::size_t n );
	static const char_type *find ( const char_type *s, std::size_t n, const char_type &a );
};
}
/// @endcond _internal

typedef std::basic_string<char, _internal::ichar_traits> istring;

template<typename CharT, typename traits>
std::list<util::istring> makeIStringList(const std::list<std::basic_string<CharT,traits>> &source){
	std::list<util::istring> ret;
	for(const std::string &ref:source)
		ret.emplace_back(ref.c_str());
	return ret;
}

}
/// @cond _internal
namespace std
{
// adapter for sending istring to normal ostream
std::ostream &operator<<( std::ostream &out, const isis::util::istring &s );
}
/// @endcond _internal
