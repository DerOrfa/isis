#include "common.hpp"
#include <cstring>

namespace isis::util
{

std::string getLastSystemError()
{
#ifdef WIN32
	std::string ret;
	LPTSTR s;
	DWORD err = GetLastError();

	if( ::FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
						 NULL,
						 err,
						 0,
						 ( LPTSTR )&s,
						 0,
						 NULL ) == 0 ) { /* failed */
		ret = std::string( "Unknown error " ) + std::to_string( err );
	} else { /* success */
		ret = s;
		ret.resize( ret.rfind( '\r' ) ); //FormatMessage appends a newline
		::LocalFree( s );
	}

	return ret;
#else
	return strerror( errno );
#endif
}
///in a list of paths shorten those paths until they are equal
std::filesystem::path getRootPath(std::list< std::filesystem::path > sources,bool sorted)
{
	//remove all entries of the list that are already equal
	if(!sorted)
		sources.sort();
	sources.erase( std::unique( sources.begin(), sources.end() ), sources.end() );
	
	// that's too short
	if( sources.empty() ) {
		LOG( Runtime, error ) << "Failed to get root path (list is empty)";
	} else if( sources.size() == 1 ) // ok, we got one unique path, return that
		return *sources.begin();
	else { // no unique path yet, try to shorten
		bool abort=true;
		for( std::filesystem::path & ref : sources ){
			if(ref.has_parent_path()){//remove last element
				ref=ref.parent_path();
				abort=false; //if at least one path can be shortened
			}
		}
		
		if(!abort){//if shortening was possible, check again for unique
			return getRootPath( sources,true );
		} else { // no more shortening possible, abort
			LOG( Runtime, error ) << "Failed to get root path for " << sources;
		}
	}
	return std::filesystem::path();
}


}
