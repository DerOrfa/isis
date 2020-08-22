// kate: indent-width 4; auto-insert-doxygen on
//
// C++ Implementation: propmap
//
// Description:
//
//
// Author:  <Enrico Reimer>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "propmap.hpp"
#include <boost/iostreams/stream.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace isis::util
{
API_EXCLUDE_BEGIN;
/// @cond _internal
namespace _internal
{

struct MapStrAdapter {
	PropertyValue operator()( const std::monostate &val )const {return PropertyValue(std::string("<<invalid node>>"));}
	PropertyValue operator()( const PropertyValue &val )const {return val;}
	PropertyValue operator()( const PropertyMap &map )const {
		return PropertyValue( std::string( "[[PropertyMap with " ) + std::to_string( map.container.size() ) + " entries]]" );
	}
};
struct RemoveEqualCheck {
	bool removeNeeded;
	explicit RemoveEqualCheck( bool _removeNeeded ): removeNeeded( _removeNeeded ) {}
	bool operator()( PropertyValue &first, const PropertyValue &second )const { // if both are Values
		if( first != second ) {
			return false;
		} else // if they're not unequal (note empty values are neither equal nor unequal)
			return removeNeeded || !first.isNeeded();
	}
	bool operator()( PropertyMap &thisMap, const PropertyMap &otherMap )const { // recurse if both are subtree
		thisMap.removeEqual( otherMap );
		return false;
	}
	template<typename T1, typename T2> bool operator()( T1 &first, const T2 &second )const {return false;} // any other case
};
struct JoinTreeVisitor {
	bool overwrite,delsource;
	PropertyMap::PathSet &rejects;
	const PropertyMap::PropPath &prefix, &name;
	JoinTreeVisitor( bool _overwrite, bool _delsource, PropertyMap::PathSet &_rejects, const PropertyMap::PropPath &_prefix, const PropertyMap::PropPath &_name )
	    : overwrite( _overwrite ), delsource(_delsource), rejects( _rejects ), prefix( _prefix ), name( _name ) {}
	bool operator()( PropertyValue &first, PropertyValue &second )const { // if both are Values
		if( first.isEmpty() || overwrite ) { // if ours is empty or overwrite is enabled
			//replace ours by the other
			if(delsource)first.transfer(second,true);
			else first = second;
			return true;
		} else { // otherwise put the other into rejected if its unequal to ours
			if( first != second && !second.isEmpty())
				rejects.insert( rejects.end(), prefix / name );
			return false;
		}
	}
	bool operator()( PropertyMap &thisMap, PropertyMap &otherMap )const { // recurse if both are subtree
		thisMap.joinTree( otherMap, overwrite, delsource, prefix / name, rejects );
		return rejects.empty();
	}
	template<typename T1, typename T2> bool operator()( T1 &first, const T2 &second )const {// reject any other case
		rejects.insert( rejects.end(), prefix / name );
		return false;
	}
};
struct FlatMapMaker {
	PropertyMap::FlatMap &out;
	const PropertyMap::PropPath &name;
	FlatMapMaker( PropertyMap::FlatMap &_out, const PropertyMap::PropPath &_name ): out( _out ), name( _name ) {}
	void operator()( const std::monostate &value )const{}
	void operator()( const PropertyValue &value )const {
		out[name] = value;
	}
	void operator()( const PropertyMap &map )const {
		for (const auto & i : map.container) {
			const auto &key=i.first;
			const PropertyMap::Node &node= i.second;
			std::visit( FlatMapMaker( out, name / key ), node.variant() );
		}
	}
};
struct TreeInvalidCheck {
	bool operator()( PropertyMap::container_type::const_reference ref ) const {
		const PropertyMap::Node &node= ref.second;
		return std::visit( *this, node.variant() );
	}//recursion
	bool operator()( const std::monostate &val )const {return false;}
	bool operator()( const PropertyValue &val )const {
		return PropertyMap::InvalidP()( val );
	}
	bool operator()( const PropertyMap &sub )const { //call my own recursion for each element
		return std::find_if( sub.container.begin(), sub.container.end(), *this ) != sub.container.end();
	}
};

/**
 * Recursive extractor-visitor.
 * Will return true, if whole tree is extractable, but will not extract.
 * Otherwise will extract all entries to "out" and return with false;
 */
struct Extractor {
	std::function<bool(const PropertyValue &prop)> &condition;
	PropertyMap &out;
	const PropertyMap::PropPath &name;
	Extractor( PropertyMap &_out, const PropertyMap::PropPath &_name, std::function<bool(const PropertyValue &prop)> &_condition )
	: out( _out ), name( _name ), condition(_condition) {}
	bool operator()( const std::monostate & )const {return false;}
	bool operator()( const PropertyValue &value )const {
		return condition(value);
	}
	bool operator()( PropertyMap &map )const {
		bool extract_me=true; // assume whole tree is "extractable" (condition is true for all downstream properties)
		std::list<PropertyMap::PropPath> to_extract;

		// check if this whole map can be extracted
		for (auto & i : map.container) {
			const PropertyMap::PropPath path=name / i.first;
			PropertyMap::Node &node=i.second;
			if(std::visit( Extractor( out, name / i.first, condition ), node.variant() )==false){ // ..
				extract_me=false; // ok this must stay, so the tree must stay
			} else {
				to_extract.emplace_back(i.first); // memorize this as to be extracted (in case not the whole tree is to be extracted)
			}
		}
		if(extract_me) // if whole tree can be extracted tell upstream
			return true;
		else {
			for (const PropertyMap::PropPath &p:to_extract) { // extract all memorized entries into branch "name" in out
				LOG_IF(!name.empty(),Debug,verbose_info) << "Extracting " << p << " into " << name;
				LOG_IF(name.empty(),Debug,verbose_info) << "Extracting " << p << " into the root of the tree";
				map.extract(p,name.empty()?out:out.touchBranch(name));
			}
			return false;
		}
	}
};


}
/// @endcond _internal
API_EXCLUDE_END;

///////////////////////////////////////////////////////////////////
// PropPath impl
///////////////////////////////////////////////////////////////////


PropertyMap::PropPath::PropPath( const char *key ): std::list<key_type>( stringToList<key_type>( istring( key ), pathSeperator ) ) {}
PropertyMap::PropPath::PropPath( const key_type &key ): std::list<key_type>( stringToList<key_type>( key, pathSeperator ) ) {}
PropertyMap::PropPath::PropPath( const std::list<key_type> &path ): std::list<key_type>( path ) {}
PropertyMap::PropPath &PropertyMap::PropPath::operator/=( const PropertyMap::PropPath &s )
{
	insert( end(), s.begin(), s.end() );
	return *this;
}
PropertyMap::PropPath PropertyMap::PropPath::operator/( const PropertyMap::PropPath &s )const {return PropPath( *this ) /= s;}

size_t PropertyMap::PropPath::length()const
{
	if( empty() )return 0;

	size_t ret = 0;
	for( const_reference ref :  *this )
	ret += ref.length();
	return ret + size() - 1;
}

std::string PropertyMap::PropPath::toString()const
{
	std::stringstream out;
	out << *this;
	return out.str();
}
///////////////////////////////////////////////////////////////////
// Contructors
///////////////////////////////////////////////////////////////////

PropertyMap::PropertyMap( const PropertyMap::container_type &src ): container( src ) {}

///////////////////////////////////////////////////////////////////
// The core tree traversal functions
///////////////////////////////////////////////////////////////////
PropertyMap::mapped_type &PropertyMap::fetchEntry( const PropPath &path )
{
	assert(!path.empty());
	return fetchEntry( container, path.begin(), path.end() );
}
PropertyMap::mapped_type &PropertyMap::fetchEntry( container_type &root, const propPathIterator at, const propPathIterator pathEnd )
{
	PropPath::const_iterator next = at;
	next++;

	if ( next != pathEnd ) {//we are not at the end of the path (a proposed leaf in the PropMap)
		const auto found = root.find( *at );
		if ( found != root.end() ) {//and we found the entry
			return fetchEntry( std::get<PropertyMap>( found->second ).container, next, pathEnd ); //continue there
		} else { // if we should create a sub-map
			//insert a empty branch (aka PropMap) at "*at" (and fetch the reference of that)
			LOG( Debug, verbose_info ) << "Creating an empty branch " << *at << " trough fetching";
			return fetchEntry( std::get<PropertyMap>( root[*at] = PropertyMap() ).container, next, pathEnd ); // and continue there (default value of the variant is PropertyValue, so init it to PropertyMap)
		}
	} else { //if its the leaf
		return root[*at]; // (create and) return that entry
	}
}

const PropertyMap::mapped_type& PropertyMap::findEntry( const PropPath &path  )const
{
	assert(!path.empty());
	return findEntryImpl<true>( container, path.begin(), path.end() );
}
PropertyMap::mapped_type& PropertyMap::findEntry( const PropPath &path  )
{
	assert(!path.empty());
	return findEntryImpl<false>( container, path.begin(), path.end() );
}

bool PropertyMap::recursiveRemove( container_type &root, const propPathIterator pathIt, const propPathIterator pathEnd )
{
	bool ret = false;

	if ( pathIt != pathEnd ) {
		propPathIterator next = pathIt;
		next++;
		const auto found = root.find( *pathIt );

		if ( found != root.end() ) {
			if ( next != pathEnd ) {
				auto &ref = std::get<PropertyMap>( found->second );
				ret = recursiveRemove( ref.container, next, pathEnd );

				if ( ref.isEmpty() )
					root.erase( found ); // remove the now empty branch
			} else {
				LOG_IF( found->second.isBranch() && !found->second.branch().isEmpty(), Debug, warning )
					<< "Deleting non-empty branch " << MSubject( found->first );
				root.erase( found );
				ret = true;
			}
		} else {
			LOG( Debug, warning ) << "Ignoring unknown entry " << *pathIt;
		}
	}

	return ret;
}


/////////////////////////////////////////////////////////////////////////////////////
// Interface for accessing elements
////////////////////////////////////////////////////////////////////////////////////

PropertyValue* PropertyMap::queryProperty(const PropertyMap::PropPath& path)
{
	return tryFindEntry<PropertyValue>( path );
}
const PropertyValue* PropertyMap::queryProperty(const PropertyMap::PropPath& path) const
{
	return tryFindEntry<PropertyValue>( path );
}
PropertyValue &PropertyMap::touchProperty( const PropertyMap::PropPath &path )
{
	return *tryFetchEntry<PropertyValue>( path );
}
PropertyValue PropertyMap::property(const PropertyMap::PropPath& path) const
{
	const auto prop = queryProperty(path);
	return prop ? *prop : PropertyValue();
}

PropertyMap* PropertyMap::queryBranch(const PropertyMap::PropPath& path)
{
	return tryFindEntry<PropertyMap>( path );
}
const PropertyMap* PropertyMap::queryBranch(const PropertyMap::PropPath& path) const
{
	return tryFindEntry<PropertyMap>( path );
}
PropertyMap& PropertyMap::touchBranch(const PropertyMap::PropPath& path)
{
	return *tryFetchEntry<PropertyMap>( path );
}
PropertyMap PropertyMap::branch( const PropertyMap::PropPath &path ) const
{
	const auto branch=queryBranch( path );
	return branch ? *branch : PropertyMap();
}

bool PropertyMap::remove( const PropPath &path )
{
	try {
		return recursiveRemove( container, path.begin(), path.end() );
	} catch( const std::bad_variant_access &e ) {
		LOG( Runtime, error ) << "Got error " << e.what() << " when removing " << path << ", aborting the removal.";
		return false;
	}
}

bool PropertyMap::remove( const PathSet &removeList, bool keep_needed )
{
	bool ret = true;
	for( PathSet::const_reference key :  removeList ) {
		auto found = tryFindEntry<PropertyValue>( key );

		if( found ) { // remove everything which is there
			if( !(found->isNeeded() && keep_needed) ) { // if its not needed or keep_need is not true
				ret &= remove( key );
			}
		} else {
			LOG( Debug, notice ) << "Can't remove property " << key << " as its not there";
		}
	}
	return ret;
}


bool PropertyMap::remove( const PropertyMap &removeMap, bool keep_needed )
{
	auto thisIt = container.begin();
	bool ret = true;

	//remove everything that is also in second
	for ( auto otherIt = removeMap.container.begin(); otherIt != removeMap.container.end(); otherIt++ ) {
		//find the closest match for otherIt->first in this (use the value-comparison-functor of PropMap)
		if ( continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { //thisIt->first == otherIt->first - so its the same property or propmap
			if ( thisIt->second.isBranch() && otherIt->second.isBranch() ) { //both are a branch => recurse
				PropertyMap &mySub = thisIt->second.branch();
				const PropertyMap &otherSub = otherIt->second.branch();
				ret &= mySub.remove( otherSub );

				if( mySub.isEmpty() ) // delete my branch, if its empty
					container.erase( thisIt++ );
			} else if( thisIt->second.isProperty() && otherIt->second.isProperty() ) {
				container.erase( thisIt++ ); // so delete this (they are equal - kind of)
			} else { // this is a leaf
				LOG( Debug, warning ) << "Not deleting branch " << MSubject( thisIt->first ) << " because its no subtree on one side";
				ret = false;
			}
		}
	}

	return ret;
}


/////////////////////////////////////////////////////////////////////////////////////
// utilities
////////////////////////////////////////////////////////////////////////////////////
bool PropertyMap::isValid() const
{
	return !_internal::TreeInvalidCheck()( *this );
}

bool PropertyMap::isEmpty() const
{
	return container.empty();
}

PropertyMap::DiffMap PropertyMap::getDifference( const PropertyMap &other ) const
{
	PropertyMap::DiffMap ret;
	diffTree( other.container, ret, PropPath() );
	return ret;
}

void PropertyMap::diffTree( const container_type& other, DiffMap& ret, const PropPath& prefix ) const
{
	auto otherIt = other.begin();

	//insert everything that is in this, but not in second or is on both but differs
	for ( auto thisIt = container.begin(); thisIt != container.end(); thisIt++ ) {
		//find the closest match for thisIt->first in other (use the value-comparison-functor of the container)
		if ( continousFind( otherIt, other.end(), *thisIt, container.value_comp() ) ) { //otherIt->first == thisIt->first - so its the same property
			if( thisIt->second.isBranch() && otherIt->second.isBranch() ) { // both are branches -- recursion step
				const PropertyMap &thisMap = thisIt->second.branch(), &refMap = otherIt->second.branch();
				thisMap.diffTree( refMap.container, ret, prefix / thisIt->first );
			} else if( thisIt->second.isProperty() && otherIt->second.isProperty() ) { // both are PropertyValue
				const auto &thisVal = std::get<PropertyValue>( thisIt->second ), &otherVal = std::get<PropertyValue>( otherIt->second );
				if(!(thisVal == otherVal) ) // if they are different
					ret.insert( // add (propertyname|(value1|value2))
					    ret.end(),      // we know it has to be at the end
					    std::make_pair(
					        prefix / thisIt->first,   //the key
					        std::make_pair( thisVal, otherVal ) //pair of both values
					    )
					);
			} else { // obviously different just stuff it in
				ret.insert( ret.end(), std::make_pair( prefix / thisIt->first, std::make_pair(
				        std::visit( _internal::MapStrAdapter(), thisIt->second.variant() ),
				        std::visit( _internal::MapStrAdapter(), otherIt->second.variant() )
				                                       ) ) );
			}
		} else { // if ref is not in the other map
			const PropertyValue firstVal = std::visit( _internal::MapStrAdapter(), thisIt->second.variant() );
			ret.insert( // add (propertyname|(value1|[empty]))
			    ret.end(),      // we know it has to be at the end
			    std::make_pair(
			        prefix / thisIt->first,
			        std::make_pair( firstVal, PropertyValue() )
			    )
			);
		}
	}

	//insert everything that is in second but not in this
	auto thisIt = container.begin();

	for ( otherIt = other.begin(); otherIt != other.end(); otherIt++ ) {
		if ( ! continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { //there is nothing in this which has the same key as ref

			const PropertyValue secondVal = std::visit( _internal::MapStrAdapter(), otherIt->second.variant() );
			ret.insert(
			    std::make_pair( // add (propertyname|([empty]|value2))
			        prefix / otherIt->first,
			        std::make_pair( PropertyValue(), secondVal )
			    )
			);
		}
	}
}

void PropertyMap::removeEqual ( const PropertyMap &other, bool removeNeeded )
{
	auto thisIt = container.begin();

	//remove everything that is also in second and equal (or also empty)
	for ( auto otherIt = other.container.begin(); otherIt != other.container.end(); otherIt++ ) {
		//find the closest match for otherIt->first in this (use the value-comparison-functor of PropMap)
		if ( continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { //thisIt->first == otherIt->first  - so its the same property

			//          if(thisIt->second.type()==typeid(PropertyValue) && otherIt->second.type()==typeid(PropertyValue)){ // if both are Values
			if( std::visit( _internal::RemoveEqualCheck( removeNeeded ), thisIt->second.variant(), otherIt->second.variant() ) ) {
				container.erase( thisIt++ ); // so delete this if both are empty _or_ equal
				continue; // keep iterator from incrementing again
			} else
				thisIt++;
		}
	}
}


PropertyMap::PathSet PropertyMap::join( const PropertyMap &other, bool overwrite)
{
	PathSet rejects;
	joinTree( const_cast<PropertyMap &>(other), overwrite, false, PropPath(), rejects );
	LOG_IF(!rejects.empty(),Debug,info) << "The properties " << MSubject(rejects) << " where rejected during the join";
	return rejects;
}
PropertyMap::PathSet PropertyMap::transfer(PropertyMap& other, int overwrite)
{
	PathSet rejects;
	joinTree( other, overwrite, true, PropPath(), rejects );
	LOG_IF(!rejects.empty(),Debug,info) << "The properties " << MSubject(rejects) << " where rejected during the transfer";
	return rejects;
}
PropertyMap::PathSet PropertyMap::transfer(PropertyMap& other, const PropPath &path, bool overwrite)
{
	auto b=other.queryBranch(path);
	PathSet rejects;
	if(b){
		rejects=transfer(*b,overwrite);
		if(rejects.empty())
			other.remove(path);
	} else
		LOG(Runtime,error) << "Branch " << path << " does not exist, won't do anything";
	return rejects;
}


void PropertyMap::joinTree( PropertyMap &other, bool overwrite, bool delsource, const PropPath &prefix, PathSet &rejects )
{
	auto thisIt = container.begin();

	for ( auto otherIt = other.container.begin(); otherIt != other.container.end(); ) { //iterate through the elements of other
		if ( continousFind( thisIt, container.end(), *otherIt, container.value_comp() ) ) { // if the element is already here
			if(
			    std::visit( _internal::JoinTreeVisitor( overwrite, delsource, rejects, prefix, thisIt->first ), thisIt->second.variant(), otherIt->second.variant() ) &&
			    delsource
			){// if the join was complete and delsource is true
				other.container.erase(otherIt++); // remove the entry from the source
			} else {
				otherIt++;
			}

		} else { // ok we dont have that - just insert it
			if(delsource){ // if we don't need the source anymore
				otherIt->second.swap(container[otherIt->first]); //swap it with the empty (because newly created) entry in the destination
				other.container.erase(otherIt++);//remove now empty entry
			} else { // insert a copy
				const std::pair<container_type::const_iterator, bool> inserted = container.insert( *otherIt );
				LOG_IF( !inserted.second, Debug, warning ) << "Failed to insert property " << MSubject( *inserted.first );
				otherIt++;
			}
		}
	}
}

PropertyMap::FlatMap PropertyMap::getFlatMap() const
{
	FlatMap buff;
	_internal::FlatMapMaker( buff, PropPath() ).operator()( *this );
	return buff;
}


bool PropertyMap::transform( const PropPath &from,  const PropPath &to, uint16_t dstID)
{
	const auto src = queryProperty( from );
	if(!src || src->isEmpty())
		return false;

	if ( src->getTypeID() == dstID ) { //same type - just rename it
		if( from != to ) // if its not at the same place anyway
			return rename(from,to);
		else
			LOG( Debug, info ) << "Not transforming " << MSubject( src ) << " into same type at same place.";
	} else { // if not the same -- convert
		LOG_IF( from == to, Debug, notice ) << "Transforming " << MSubject( src ) << " in place.";
		PropertyValue buff= src->copyByID( dstID );

		if( !buff.isEmpty() ){
			touchProperty( to ).swap(buff);
			LOG_IF(!buff.isEmpty(), Runtime,warning) << "There already was a property " << std::make_pair(to,buff) << " it will be overwritten";
			if(from!=to)remove( from );
			return true;
		}
	}
	return false;
}

bool PropertyMap::operator==(const PropertyMap &other) const {return container == other.container;}

bool PropertyMap::operator!=(const PropertyMap &other) const {return container != other.container;}


PropertyMap::PathSet PropertyMap::getKeys()const   {return genKeyList<TrueP>();}
PropertyMap::PathSet PropertyMap::getMissing()const {return genKeyList<InvalidP>();}

PropertyMap::PathSet PropertyMap::localBranches() const
{
	return getLocal<PropertyMap>();
}
PropertyMap::PathSet PropertyMap::localProps() const
{
	return getLocal<PropertyValue>();
}


void PropertyMap::addNeeded( const PropPath &path )
{
	touchProperty( path ).needed() = true;
}


size_t PropertyMap::hasProperty( const PropPath &path ) const
{
	const auto ref = queryProperty( path );
	return ref ? ref->size():0;
}

PropertyMap::PropPath PropertyMap::find( const key_type &key, bool allowProperty, bool allowBranch ) const
{
	const PropPath name(key);
	if( name.empty() ) {
		LOG( Debug, error ) << "Search key " << MSubject( name ) << " is invalid, won't search";
		return key_type();
	}

	LOG_IF( name.size() > 1, Debug, warning ) << "Stripping search key " << MSubject( name ) << " to " << name.back();

	// if the searched key is on this brach return its name
	auto found = container.find( name.back() );

	if( found != container.end() &&
	    ( ( found->second.isProperty() && allowProperty ) || ( found->second.isBranch() && allowBranch ) )
	  ) {
		return found->first;
	} else { // otherwise search in the branches
		for( container_type::const_reference ref :  container ) {
			if( ref.second.isBranch() ) {
				const PropPath foundPath = ref.second.branch().find( name.back(), allowProperty, allowBranch );

				if( !foundPath.empty() ) // if the key is found abort search and return it with its branch-name
					return PropPath( ref.first ) / foundPath;
			}
		}
	}

	return PropPath(); // nothing found
}

bool PropertyMap::hasBranch( const PropPath &path ) const
{
	const auto q=queryBranch( path );
	return q && !q->isEmpty();
}

bool PropertyMap::rename( const PropPath &oldname,  const PropPath &newname, bool overwrite )
{
	mapped_type& old_e = findEntry( oldname );

	if ( !old_e ) {//abort if oldname is not there
		LOG( Runtime, error ) << "Cannot rename " << oldname << " it does not exist";
		return false;
	}
	if(newname != oldname){
		mapped_type &new_e = fetchEntry( newname );
		const bool empty=std::visit(IsEmpty(), new_e.variant());
		if(!empty && !overwrite){ //abort if we're no supposed to overwrite'
			LOG(Runtime,warning) << "Not overwriting " << std::make_pair( newname, new_e ) << " with " << *old_e;
			return false;
		}
		LOG_IF( !empty , Runtime, warning ) << "Overwriting " << std::make_pair( newname, new_e ) << " with " << *old_e;
		new_e.swap(old_e);
		remove( oldname ); //can only fail if oldname is not there -- and if it wasn't we'd have aborted already
	} else { //if its a lexical rename get the data out of the map
		mapped_type buff;old_e.swap(buff);
		remove( oldname );
		fetchEntry(newname).swap(buff);//and re-insert it with newname
	}
	return true;
}

void PropertyMap::removeUncommon( PropertyMap &common )const
{
#pragma message("getDifference is waste of time here")
	const DiffMap difference = common.getDifference( *this );
	for( const DiffMap::value_type & ref :  difference ) {
		if ( ! ref.second.first.isEmpty() ) {
			LOG( Debug, verbose_info ) << "Detected difference in " << ref << " removing from common";
			common.remove( ref.first );//if there is something in common, remove it
		}
	}
}

bool PropertyMap::insert(const std::pair<std::string,PropertyValue> &p){
	return insert(std::make_pair(PropPath(p.first.c_str()),p.second));
}

bool PropertyMap::insert(const std::pair<PropPath,PropertyValue> &p){
	PropertyValue &entry= touchProperty(p.first);
	if(entry.isEmpty()){
		entry=p.second;
		return true;
	} else
		return false;
}

void PropertyMap::extract(const PropPath &p,PropertyMap &dst){
	Node &found=findEntry(p);
	if(found){
		Node &d_node=dst.fetchEntry(p);
		assert(std::visit(IsEmpty(),d_node.variant()));
		d_node.swap(found);
		remove(p);
	}
}

PropertyMap PropertyMap::extract_if(std::function<bool(const PropertyValue &p)> condition){
	PropertyMap dst;
	if(_internal::Extractor( dst, PropPath(), condition ).operator()( *this )){
		std::swap(this->container,dst.container);
		this->container.clear();
	}
	return dst;
}

void PropertyMap::deduplicate(std::list<std::shared_ptr<PropertyMap>> maps){

	assert(!maps.empty());

	util::PropertyMap common=*maps.front();  //copy all props from the first chunk
	auto p=maps.begin();

	// common now has all props (from the first chunk)
	for(++p;p!=maps.end();++p)
		(*p)->removeUncommon( common );//remove everything which isn't common from common

	//then remove remaining common props from the chunks
	for ( auto &p: maps )
		p->remove( common, false ); //this _won't_ keep needed properties - so from here on the "maps" are invalid

	const PathSet rej = this->transfer(common);
	LOG_IF(!rej.empty(),Debug,error) << "Some props where rejected when joining the commons into the me (" << rej << ")";
}

std::ostream &PropertyMap::print( std::ostream &out, bool label )const
{
	FlatMap buff = getFlatMap();
	size_t key_len = 0;

	for ( auto i = buff.begin(); i != buff.end(); i++ )
		if ( key_len < i->first.length() )
			key_len = i->first.length();

	for ( auto i = buff.begin(); i != buff.end(); i++ )
		out << i->first << std::string( key_len - i->first.length(), ' ' ) + ":" << i->second.toString( label ) << std::endl;

	return out;
}

PropertyValue& PropertyMap::setValue(const PropPath &path, const Value &val, const std::optional<size_t> &at){
	PropertyValue &ret = touchProperty( path );
	const auto index=at.value_or(0);

	if( ret.size() <= index ) {
		LOG_IF(index, Debug, info ) << "Extending " << std::make_pair( path, ret ) << " to fit length " << at.value_or(0); //don't tell about extending empty property
		ret.resize( index + 1, val ); //resizing will insert the value
	} else {
		if( ret[index].apply( val ) ){ // override same type or convert if possible
			LOG_IF( ret.getTypeID()!=val.typeID(), Debug, info ) << "Storing " << val.toString( true ) << " as " << ret.toString( true ) << " as Property already has that type";
		} else {
			LOG( Runtime, error ) << "Property " << path << " is already set to " << ret.toString( true ) << " won't overwrite with " << val.toString( true );
		}
	}

	return ret;
}


void PropertyMap::readPtree(const boost::property_tree::ptree &tree,bool skip_empty){
	for(const auto &p:tree){
		const auto &key=p.first;
		const auto &entry=p.second;
		if(entry.empty()){
			if(!entry.data().empty() || !skip_empty){
				mapped_type &n = fetchEntry( key.c_str() );
				if(std::visit(IsEmpty(),n.variant())){
					n=PropertyValue(entry.data());
				} else if(n.isProperty()){ //OK we got a property map all is good
					std::get<PropertyValue>(n)=entry.data();
				} else {
					LOG(Runtime,warning) << "Will not store " << key << " as PropertyValue as it already exists as branch " << n;
				}
			}
		} else {
			mapped_type &n = fetchEntry( key.c_str() );
			if(std::visit(IsEmpty(),n.variant())){
				n=PropertyMap();
			}
			if(n.isBranch()){ //OK we got a property map all is good
				n.branch().readPtree(entry);
			} else  {
				LOG(Runtime,warning) << "Will not store " << key << " as branch as it already exists as property " << n;
			}
		}
	}
}

void PropertyMap::readXML(const uint8_t* streamBegin, const uint8_t* streamEnd, int flags){
	typedef  boost::iostreams::basic_array_source<std::streambuf::char_type> my_source_type; // must be compatible to std::streambuf

	boost::iostreams::stream<my_source_type> stream;
	stream.open(my_source_type((const std::streambuf::char_type*)streamBegin,(const std::streambuf::char_type*)streamEnd));
	readXML(stream,flags);
}
void PropertyMap::readXML(std::basic_istream<char> &stream,int flags){
	boost::property_tree::ptree pt;
	boost::property_tree::read_xml(stream,pt,flags);
// 	boost::property_tree::write_xml(std::cout,pt,boost::property_tree::xml_writer_settings<std::string>(' ',4));std::cout<<std::endl;
	readPtree(pt,true);
}
PropertyMap::Node& PropertyMap::nullnode(){
	static Node node;
	return node;
}
/// @cond _internal
bool PropertyMap::TrueP::operator()( const PropertyValue &/*ref*/ ) const {return true;}
bool PropertyMap::InvalidP::operator()( const PropertyValue &ref ) const {return ref.isNeeded() && ref.isEmpty();}
bool PropertyMap::EmptyP::operator()(const PropertyValue& ref) const {return ref.isEmpty();};

/// @endcond _internal

}
