#include <iostream>
#include "istring.hpp"

#ifdef _MSC_VER
#include <Windows.h>
#else
#include <cstring>
#endif
#include <algorithm>

/// @cond _internal
namespace isis::util::_internal
{

std::locale const ichar_traits::loc = std::locale( "C" );

int ichar_traits::compare( const char *s1, const char *s2, size_t n )
{
#ifdef _MSC_VER
	return lstrcmpiA( s1, s2 ); //@todo find some with length parameter
#else
	return strncasecmp( s1, s2, n );
#endif
}

bool ichar_traits::eq( const char &c1, const char &c2 )
{
	return std::tolower( c1, loc ) == std::tolower( c2, loc );
}

bool ichar_traits::lt( const char &c1, const char &c2 )
{
	return std::tolower( c1, loc ) < std::tolower( c2, loc );
}

const char *ichar_traits::find( const char *s, size_t n, const char &a )
{
	const auto lowA = (char)std::tolower( a );

	if( lowA == std::toupper( a ) ) { // if a has no cases we can do naive search
		return std::find( s, s + n, a );
	} else for( size_t i = 0; i < n; i++, s++ ) {
			if( std::tolower( *s ) == lowA )
				return s;
		}

	return nullptr;
}


}
/// @endcond _internal

std::ostream & std::operator<<(std::ostream& out, const isis::util::istring& s)
{
	return out.write(s.data(),s.size());
}
