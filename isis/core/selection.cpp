/*
 *  selection.cpp
 *  isis
 *
 *  Created by Enrico Reimer on 03.04.10.
 *  Copyright 2010 cbs.mpg.de. All rights reserved.
 *
 */

#include "selection.hpp"

namespace isis::util
{

Selection::Selection(std::initializer_list<std::string> entries, const char *init_val )
{
	MapType::mapped_type ID = 0;
	for( const auto &ref : entries ) {
		const MapType::value_type pair( {ref.begin(),ref.end()}, ID++ );

		if( ! ent_map.insert( pair ).second ) {
			LOG( Debug, error ) << "Entry " << pair << " could not be inserted";
		}
	}

	if( init_val[0] )
		set( init_val );
}

Selection::operator bool() const
{
	return static_cast<bool>(m_set);
}

bool Selection::idExists(unsigned int entry)
{
	return std::find_if(ent_map.begin(),ent_map.end(),[entry](auto &v){return v.second==entry;})!=ent_map.end();
}
void Selection::set( unsigned int entry )
{
	LOG_IF(!idExists(entry),Debug,error) << "The value you're setting doesn't exist in this selection";
	m_set = entry;
}
bool Selection::set( const char *entry )
{
	auto found = ent_map.find( entry );

	if ( found != ent_map.end() ) {
		m_set = found->second;
		return true;
	} else {
		LOG( Runtime, error ) << "Failed to set " << MSubject( entry ) << ", valid values are " << getEntries();
		return false;
	}
}

bool Selection::operator==( const Selection &ref )const{return comp_op(ref,std::equal_to<>());}
bool Selection::operator!=( const Selection &ref )const{return comp_op(ref,std::not_equal_to<>());}

bool Selection::operator==( std::basic_string_view<char,util::_internal::ichar_traits> ref )const{
	auto [valid, iter] = stringCompareCheck(ref);
	return valid && iter->second == *m_set;
}
bool Selection::operator!=( std::basic_string_view<char,util::_internal::ichar_traits> ref )const{
	auto [valid, iter] = stringCompareCheck(ref);
	return valid && iter->second != *m_set;
}


bool Selection::operator<( const Selection &ref )const{return comp_op(ref,std::less<>());}

bool Selection::operator>( const Selection &ref )const{return comp_op(ref,std::less<>());}

std::list<util::istring> Selection::getEntries()const
{
	std::list<MapType::value_type> buffer(ent_map.begin(),ent_map.end());
	buffer.sort([](const MapType::value_type &v1,const MapType::value_type &v2){return v1.second<v2.second;});
	
	std::list<util::istring> ret;
	for( MapType::value_type &ref :  buffer ) {
		ret.push_back( ref.first );
	}
	return ret;
}
std::pair<bool, Selection::MapType::const_iterator>
Selection::stringCompareCheck(std::basic_string_view<char, util::_internal::ichar_traits> ref) const
{
	if(m_set){
		auto found=ent_map.find(ref);
		LOG_IF(found!=ent_map.end(), Runtime,warning) << "The string you're comparing to does not exist in this map";
		return {found!=ent_map.end(), found};
	} else {
		LOG(Debug,warning) << "Comparing to unset Selection is always false";
		return {false,{}};
	}
}
}
