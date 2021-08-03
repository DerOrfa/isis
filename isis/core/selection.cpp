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

Selection::operator int()const {return m_set;}
Selection::operator util::istring()const
{
	if(m_set==std::numeric_limits<MapType::mapped_type>::max())
		return "<<NOT_SET>>";
	else {
		for( MapType::const_reference ref :  ent_map ) {
			if ( ref.second == m_set )
				return ref.first;
		}
		assert(false); // m_set should either be in the map or 0
		return "<<UNKNOWN>>";
	}
}
Selection::operator std::string()const
{
	util::istring buff = *this;
	return { buff.begin(), buff.end() };
}

bool Selection::idExists(unsigned int entry)
{
	return std::find_if(ent_map.begin(),ent_map.end(),[entry](auto &v){return v.second==entry;})!=ent_map.end();
}
void Selection::set( unsigned int entry )
{
	LOG_IF(!idExists(entry),Debug,error) << "The value you're trying to set doesn't exist in this selection";
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

bool Selection::operator==( const Selection &ref )const{return comp_op(ref,std::equal_to<int>());}
bool Selection::operator==( const std::string_view &ref )const{return std::equal_to<util::istring>()(*this, {ref.begin(),ref.end()});}

bool Selection::operator!=( const Selection &ref )const{return comp_op(ref,std::not_equal_to<int>());}
bool Selection::operator!=( const std::string_view &ref )const{return std::not_equal_to<util::istring>()(*this, {ref.begin(),ref.end()});}

bool Selection::operator<( const Selection &ref )const{return comp_op(ref,std::less<int>());}

bool Selection::operator>( const Selection &ref )const{return comp_op(ref,std::less<int>());}

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
}
