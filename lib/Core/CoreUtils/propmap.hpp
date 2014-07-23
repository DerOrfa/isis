//
// C++ Interface: propmap
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef ISISPROPMAP_HPP
#define ISISPROPMAP_HPP

#include <map>
#include <string>

#include "common.hpp"
#include "property.hpp"
#include "log.hpp"
#include "istring.hpp"
#include <set>
#include <algorithm>
#include <boost/variant/variant.hpp>
#include <boost/variant/get.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/optional.hpp>

namespace isis
{
namespace util
{
/// @cond _internal
namespace _internal
{
struct MapStrAdapter;
struct JoinTreeVisitor;
struct SwapVisitor;
struct FlatMapMaker;
struct TreeInvalidCheck;
}
/// @endcond 
/**
 * This class forms a mapping tree to store all kinds of properties (path : value), where value is a util::PropertyValue-container holding the value(s) of the property (this may be empty/unset)
 *
 * There are separate access functions for branches and properties.
 * Trying to access a branch as a property value or to access a property value as a branch will cause error messages and give empty results.
 *
 * Paths can be created from other paths and from strings (c-strings and util::istring, but not std::string).
 * So both can be used for functions which expect paths, but the usage of c-strings is slower.
 *
 * To describe the minimum of needed metadata needed by specific data structures / subclasses
 * properties can be marked as "needed" and there are functions to verify that those are not empty.
 */
class PropertyMap
{
public:
/// @cond _internal
	friend struct _internal::MapStrAdapter;
	friend struct _internal::JoinTreeVisitor;
	friend struct _internal::FlatMapMaker;
	friend struct _internal::TreeInvalidCheck;
/// @endcond
	typedef std::map<util::istring, boost::variant<PropertyValue, PropertyMap> > container_type;
	/// type of the keys forming a path
	typedef container_type::key_type key_type;
	typedef container_type::mapped_type mapped_type;

	/// "Path" type used to locate entries in the tree
	struct PropPath: public std::list<key_type> {
		static const char pathSeperator = '/';
		PropPath();
		PropPath( const char *key );
		PropPath( const key_type &key );
		PropPath( const std::list<key_type> &path );
		PropPath operator/( const PropPath &s )const;
		PropPath &operator/=( const PropPath &s );
		PropPath operator/( key_type s )const;
		PropPath &operator/=( key_type s );
		bool operator==( const key_type &s )const {return *this == PropPath( s );}
		bool operator!=( const key_type &s )const {return *this != PropPath( s );}
		size_t length()const;
	};

	///a list to store keys only (without the corresponding values)
	typedef std::set<PropPath> PathSet;
	///a flat map matching keys to pairs of values
	typedef std::map<PropPath, std::pair<PropertyValue, PropertyValue> > DiffMap;
	///a flat map, matching complete paths as keys to the corresponding values
	typedef std::map<PropPath, PropertyValue> FlatMap;


protected:
	typedef PropPath::const_iterator propPathIterator;
	container_type container;

/// @cond _internal
API_EXCLUDE_BEGIN;
	/////////////////////////////////////////////////////////////////////////////////////////
	// internal predicats
	/////////////////////////////////////////////////////////////////////////////////////////
	/// allways true
	struct TrueP { bool operator()( const PropertyValue &ref )const;};
	/// true when the Property is needed and empty
	struct InvalidP { bool operator()( const PropertyValue &ref )const;};

	/////////////////////////////////////////////////////////////////////////////////////////
	// internal functors
	/////////////////////////////////////////////////////////////////////////////////////////
	template<class Predicate> struct WalkTree: public boost::static_visitor<void> {
		PathSet &m_out;
		const PropPath &name;
		WalkTree( PathSet &out, const PropPath &prefix ): m_out( out ), name( prefix ) {}
		void operator()( container_type::const_reference ref ) const {boost::apply_visitor( WalkTree( m_out, name / ref.first ), ref.second );} //recursion
		void operator()( const PropertyValue &val )const {
			if ( Predicate()( val ) )
				m_out.insert( m_out.end(), name );
		}
		void operator()( const PropertyMap &sub )const { //call my own recursion for each element
			std::for_each( sub.container.begin(), sub.container.end(), *this );
		}
	};
	template<typename ITER> struct Splicer: public boost::static_visitor<void> {
		const ITER &first, &last;
		const PropPath &name;
		Splicer( const ITER &_first, const ITER &_last, const PropPath &_name ): first( _first ), last( _last ), name( _name ) {}
		void operator()( const container_type::value_type &pair )const {
			boost::apply_visitor( Splicer<ITER>( first, last, name / pair.first ), pair.second );
		}
		void operator()( PropertyValue val )const {
			const size_t blocks = std::distance( first, last );
			assert( blocks );

			if( val.size() % blocks ) { // just copy all which cannot be properly spliced to the destination
				LOG_IF( val.size() > 1, Runtime, warning ) << "Not splicing non scalar property " << MSubject( name ) << " because its length "
						<< MSubject( val.size() ) << " doesn't fit the amount of targets(" << MSubject( blocks ) << ")"; //tell the user if its no scalar

				for( ITER i = first; i != last; i++ )
					i->property( name ) = val;
			} else {
				ITER i = first;
				BOOST_FOREACH( const PropertyValue & splint, val.splice( val.size() / blocks ) ) {
					assert( i != last );
					( i++ )->property( name ) = splint;
				}
			}
		}
		void operator()( const PropertyMap &sub )const { //call my own recursion for each element
			std::for_each( sub.container.begin(), sub.container.end(), *this );
		}
	};
	struct IsEmpty : boost::static_visitor<bool>
	{
		template <typename T> bool operator()( T & operand ) const{
			return operand.isEmpty();
		}
	};
API_EXCLUDE_END;
/// @endcond _internal

	/////////////////////////////////////////////////////////////////////////////////////////
	// internal tool-backends
	/////////////////////////////////////////////////////////////////////////////////////////
	/// internal recursion-function for join
	void joinTree( PropertyMap& other, bool overwrite, bool delsource, const PropPath& prefix, PathSet& rejects );
	/// internal recursion-function for diff
	void diffTree( const container_type &other, DiffMap &ret, const PropPath &prefix ) const;

	/// internal recursion-function for remove
	static bool recursiveRemove( container_type &root, const propPathIterator pathIt, const propPathIterator pathEnd )throw( boost::bad_get );

	template <typename MAPPED,typename CONTAINER> static boost::optional<MAPPED &> findEntryImpl( CONTAINER &root, const propPathIterator at, const propPathIterator pathEnd )throw( boost::bad_get )
	{
		propPathIterator next = at;
		next++;
		BOOST_AUTO(found, root.find( *at ));

		if ( next != pathEnd ) {//we are not at the end of the path (aka the leaf)
			if ( found != root.end() ) {//and we found the entry
				return findEntryImpl<MAPPED,CONTAINER>( boost::get<PropertyMap>( found->second ).container, next, pathEnd ); //continue there
			}
		} else if ( found != root.end() ) {// if its the leaf and we found the entry
			return found->second; // return that entry
		}

		return boost::optional<MAPPED &>();
	}


protected:
/// @cond _internal
	template<typename T> class NeededsList: public std::list<PropPath>
	{
	public:
		NeededsList() {
			const std::list< PropertyMap::key_type > buff = util::stringToList<PropertyMap::key_type>( T::neededProperties ); //@todo really bad voodoo
			assign( buff.begin(), buff.end() );
		}
		void applyTo( PropertyMap &props ) {
			BOOST_FOREACH( const PropPath & ref, *this ) {
				props.addNeeded( ref );
			}
		}
	};
	template<typename T> boost::optional<T &> refValueAsImpl( const PropPath &path, const boost::optional<size_t> &at ) {
		const boost::optional< const PropertyValue & > found = tryFindEntry<PropertyValue>( path );

		if( found && found->size()>at.get_value_or(0) ) {// apparently it already has a value so lets try use that
			if( !found->is<T>() ) {
				if( !transform<T>( path, path ) ) {// convert to requested type
					LOG( Runtime, warning ) << "Conversion of Property " << path << " from " << util::MSubject( found->getTypeName() ) << " to "
											<< util::MSubject( util::Value<T>::staticName() ) << " failed";
					return boost::optional<T &>();
				}
			}
			assert( found->is<T>() );
			return at ?
				const_cast<PropertyValue &>( *found ).at(*at).castTo<T>():
				const_cast<PropertyValue &>( *found ).castTo<T>(); // use single value ops, if at was not given
		}

		return boost::optional<T &>();
	}
	template<typename T> T getValueAsImpl( const PropPath &path, const boost::optional<size_t> &at )const {
		const boost::optional< const PropertyValue & > ref = tryFindEntry<PropertyValue>( path );

		if( !ref ){
			LOG( Runtime, warning ) << "Property " << MSubject( path ) << " doesn't exist returning " << MSubject( Value<T>() );
			return T();
		} else if (ref->size()<=at.get_value_or(0)){
			LOG_IF(at, Runtime, warning ) << "Property " << MSubject( std::make_pair( path, ref ) ) << " does exist, but index " << MSubject( at ) << " is out of bounds. returning " << MSubject( Value<T>() );
			LOG_IF(!at, Runtime, warning ) << "Property " << MSubject( std::make_pair( path, ref ) ) << " is empty. returning " << MSubject( Value<T>() );
			return T();
		} else
			return at ?
				ref->at( *at ).as<T>():
				ref->as<T>();// use single value ops, if at was not given
	}

/// @endcond _internal
	/////////////////////////////////////////////////////////////////////////////////////////
	// rw-backends
	/////////////////////////////////////////////////////////////////////////////////////////
	/**
	 * Follow a "Path" to a property to get/make it.
	 * This will create branches and the property on its way if necessary.
	 */
	container_type::mapped_type &fetchEntry( const PropPath &path ) throw( boost::bad_get );
	/**
	 * \copydoc fetchEntry( const PropPath &path )
	 * Will work on the container given as root.
	 */
	static container_type::mapped_type &fetchEntry( container_type &root, const propPathIterator at, const propPathIterator pathEnd ) throw( boost::bad_get );

	/**
	 * Find property following the given "path".
	 * If the "path" or the property does not exist NULL is returned.
	 * \note this is the const version of \link fetchEntry( const PropPath &path ) \endlink, so it won't modify the map.
	 */
	boost::optional<const PropertyMap::mapped_type &> findEntry( const PropPath &path )const throw( boost::bad_get );
	boost::optional<PropertyMap::mapped_type &> findEntry( const PropPath &path  )throw( boost::bad_get );

	template<typename T> boost::optional<const T &> tryFindEntry( const PropPath &path )const {
		try {
			const boost::optional<const PropertyMap::mapped_type &> ref = findEntry( path );

			if( ref )
				return boost::get<T>( *ref );
		} catch ( const boost::bad_get &e ) {
			LOG( Runtime, error ) << "Got errror " << e.what() << " when accessing " << MSubject( path );
		}

		return  boost::optional<const T &>();
	}
	template<typename T> boost::optional<T &> tryFetchEntry( const PropPath &path ) {
		try {
			mapped_type &n = fetchEntry( path );
			if(n.type()!=typeid(T) && boost::apply_visitor(IsEmpty(),n)) //if target is empty but of wrong type 
				boost::get<T>( n = T() ); // reset it to empty with right type
			return boost::get<T>( n );
		} catch( const boost::bad_get &e ) {
			LOG( Runtime, error ) << "Got errror " << e.what() << " when accessing " << MSubject( path );
		}

		return  boost::optional<T &>();
	}


	/// create a list of keys for every entry for which the given scalar predicate is true.
	template<class Predicate> PathSet genKeyList()const {
		PathSet k;
		std::for_each( container.begin(), container.end(), WalkTree<Predicate>( k, PropPath() ) );
		return k;
	}
	/**
	 * Adds a property with status needed.
	 * \param path identifies the property to be added or if already existsing to be flagged as needed
	 */
	void addNeeded( const PropPath &path );

	/**
	 * Remove properties from another tree that are in both, but not equal
	 * For every entry of the tree this checks if it is also in the given other tree and removes it there if its not equal.
	 * This is done by:
	 * - generating a difference (using diff) between the current common and the tree
	 *   - the resulting diff_map contains all properties which has been in common, but are not equal in the tree
	 * - these newly diffent properties are removed from common.
	 * \param common reference of the other (common) tree
	 */
	void removeUncommon( PropertyMap& common )const;

public:
	/////////////////////////////////////////////////////////////////////////////////////////
	// constructors
	/////////////////////////////////////////////////////////////////////////////////////////
	PropertyMap();
	PropertyMap( const container_type &cont );

	/////////////////////////////////////////////////////////////////////////////////////////
	// Common rw-accessors
	/////////////////////////////////////////////////////////////////////////////////////////
	/**
	 * Access the property referenced by the path.
	 * If the property does not exist, a failed assertion will be raised.
	 * As well as for accessing errors like the given path is a branch instead of a property.
	 * \param path the path to the property
	 * \returns a reference to the property
	 */
	const PropertyValue &property( const PropPath &path )const;

	/**
	 * Access the property referenced by the path, create it if its not there.
	 * Accessing errors like if the given path exists as a branch already will fail an assertion.
	 * \param path the path to the property
	 * \returns a reference to the property
	 */
	PropertyValue &property( const PropPath &path );

	/**
	 * Access the branch referenced by the path, create it if its not there.
	 * Accessing errors like if the given path exists as a property already will fail an assertion.
	 * \param path the path to the branch
	 * \returns a reference to the branching PropertyMap
	 */
	PropertyMap &branch( const PropPath &path );

	/**
	 * Access the branch referenced by the path.
	 * If the branch does not exist, a failed assertion will be raised.
	 * As well as for accessing errors like the given path is a property instead of a branch.
	 * \param path the path to the branch
	 * \returns a reference to the branching PropertyMap
	 */
	const PropertyMap &branch( const PropPath &path )const;

	/**
	 * Remove the property adressed by the path.
	 * This actually only removes properties.
	 * Non-empty branches are not deleted.
	 * And if an branch becomes empty after deletion of its last entry, it is deleted automatically.
	 * \param path the path to the property
	 * \returns true if successful, false otherwise
	 */
	bool remove( const PropPath &path );

	/**
	 * remove every property which is also in the given tree (regardless of the value)
	 * \param removeMap the tree of properties to be removed
	 * \param keep_needed when true needed properties are kept even if they would be removed otherwise
	 * \returns true if all properties removed succesfully, false otherwise
	 */
	bool remove( const PropertyMap &removeMap, bool keep_needed = false );

	/**
	 * remove every property which is also in the given list (regardless of the value)
	 * \param removeList a list of paths naming the properties to be removed
	 * \param keep_needed when true needed properties are kept even if they would be removed otherwise
	 * \returns true if all properties removed succesfully, false otherwise
	 */
	bool remove( const PathSet &removeList, bool keep_needed = false );

	/**
	 * Remove every PropertyValue which is also in the other PropertyMap and where operator== returns true.
	 * \param other the other property tree to compare to
	 * \param removeNeeded if a property should also be deleted it is needed
	 */
	void removeEqual( const PropertyMap &other, bool removeNeeded = false );

	/**
	 * check if a property is available
	 * \param path the path to the property
	 * \note the return value is a boost::optional thus the actual PropertyValue can be accessed via the * or -> operators (if there is one of course)
	 * \returns true if the given property does exist and is not empty, false otherwise
	 */
	boost::optional< const PropertyValue & > hasProperty( const PropPath &path )const;

	/**
	 * Search for a property/branch in the whole Tree.
	 * \param key the single key for the branch/property to search for (paths will be stripped to the rightmost key)
	 * \param allowProperty if false the search will ignore properties
	 * \param allowBranch if false the search will ignore branches (it will still search into the branches, but the branches themself won't be considered a valid finding)
	 * \returns full path to the property (including the properties name) if it is found, empty string elsewhise
	 */
	PropPath find( const key_type &key, bool allowProperty = true, bool allowBranch = false )const;

	/**
	 * check if branch of the tree is available
	 * \param path the path to the branch
	 * \note the return value is a boost::optional thus the actual PropertyMap can be accessed via the * or -> operators (if there is one of course)
	 * \returns true if the given branch does exist and is not empty, false otherwise
	 */
	boost::optional< const PropertyMap & > hasBranch( const PropPath &path )const;

	////////////////////////////////////////////////////////////////////////////////////////
	// tools
	/////////////////////////////////////////////////////////////////////////////////////////
	/**
	* Check if every property marked as needed is set.
	* \returns true if ALL needed properties are NOT empty, false otherwhise.
	*/
	bool isValid()const;

	/// \returns true if the PropertyMap is empty, false otherwhise
	bool isEmpty()const;

	/**
	 * Get a list of the paths of all properties.
	 * \returns a flat list of the paths to all properties in the PropertyMap
	 */
	PathSet getKeys()const;

	/**
	 * Get a list of missing properties.
	 * \returns a list of the paths for all properties which are marked as needed and but are empty.
	 */
	PathSet getMissing()const;

	/**
	 * Get a difference map of this tree and another.
	 * Out of the names of differing properties a mapping from paths to std::pair\<PropertyValue,PropertyValue\> is created with following rules:
	 * - if a Property is empty in this tree but set in second,
	 *   - it will be added with the first PropertyValue emtpy and the second PropertyValue
	 *   taken from second
	 * - if a Property is set in this tree but empty in second,
	 *   - it will be added with the first PropertyValue taken from this and the second PropertyValue empty
	 * - if a Property is set in both, but not equal,
	 *   - it will be added with the first PropertyValue taken from this and the second PropertyValue taken from second
	 * - if a Property is set in both and equal, or is empty in both,
	 *   - it wont be added
	 * \param second the other tree to compare with
	 * \returns a map of property paths and pairs of the corresponding different values
	 */
	DiffMap getDifference( const PropertyMap &second )const;

	/**
	 * Add Properties from another property map.
	 * \param other the other tree to join with
	 * \param overwrite if existing properties shall be replaced
	 * \returns a list of the rejected properties that couldn't be inserted, for success this should be empty
	 */
	PropertyMap::PathSet join( const PropertyMap& other, bool overwrite = false );

	/**
	 * Transfer all Properties from another PropertyMap.
	 * The source should be empty afterwards.
	 * \param other the other tree to transfer from
	 * \param overwrite if existing properties shall be replaced
	 * \returns a list of the rejected properties that couldn't be inserted, for success this should be empty
	 */
	PropertyMap::PathSet transfer( PropertyMap& other, int overwrite = 0 ); //use int to prevent implicit conversion from static string (PropPath) to bool

	/**
	 * Transfer a single entry / branch from another PropertyMap.
	 * The transferred entry / branch  will be removed from the source.
	 * \param other the other tree to transfer from
	 * \param other_path the entry/subtree that should be transferred
	 * \param overwrite if existing properties shall be replaced
	 * \returns a list of the rejected properties that couldn't be inserted, for success this should be empty
	 */
	PropertyMap::PathSet transfer( PropertyMap& other, const PropPath &other_path, bool overwrite = false );

	/**
	 * Transform an existing PropertyValue into another.
	 * Converts the value of the given PropertyValue into the requested type and stores it with the given new path.
	 * \param from the path of the property to be transformed
	 * \param to the path for the new property
	 * \param dstID the type-ID of the new property value
	 * \returns true if the transformation was done, false it failed
	 */
	bool transform( const PropPath& from, const PropPath& to, uint16_t dstID);

	/**
	 * Transform an existing PropertyValue into another (statically typed version).
	 * Converts the value of the given PropertyValue into the requested type and stores it with the given new path.
	 * A compile-time check is done to test if the requested type is available.
	 * \param from the path of the PropertyValue to be transformed
	 * \param to the path for the new PropertyValue
	 * \returns true if the transformation was done, false if it failed
	 */
	template<typename DST> bool transform( const PropPath &from, const PropPath &to) {
		checkType<DST>();
		return transform( from, to, Value<DST>::staticID);
	}

	/**
	 * Copy-Splice this map into a list of other PropertyMaps.
	 * This will copy all scalar Properties equally into a list of destination maps represented by iterators.
	 * Multi-Value Properties will be split up, if their length fits.
	 * \param first start of the destination list
	 * \param last end of the destination list
	 * \note dereferencing ITER must result in PropertyMap&
	 */
	template<typename ITER> void splice( ITER first, ITER last )const {
		std::for_each( container.begin(), container.end(), Splicer<ITER>( first, last, PropPath() ) );
	}

	//////////////////////////////////////////////////////////////////////////////////////
	// comparison
	//////////////////////////////////////////////////////////////////////////////////////
	bool operator==( const PropertyMap &other )const {return container == other.container;}
	bool operator!=( const PropertyMap &other )const {return container != other.container;}

	//////////////////////////////////////////////////////////////////////////////////////
	// Additional get/set - Functions
	//////////////////////////////////////////////////////////////////////////////////////
	/**
	 * Set the given property to a given value/type.
	 * The needed flag (if set) will be kept.
	 * The property will be set to the one given value if
	 * - the property is empty or
	 * - the property is already set to one value of the same type (value will be overwritten)
	 * - the property is already set to another value and the new value can be converted to that type (value will be overwritten but type will be kept)
	 * The property will not be set and an error will be send to Runtime if
	 * - the property is already set to one value of another type an and no conversion is available
	 * - the property stores more that one value (use setValueAs( const PropPath &path, size_t at, const T &val ) instead)
	 * \note This is a single value operation. So warning is send to Debug, if accessing a multivalue property.
	 * \code
	 * setValueAs("MyPropertyName", isis::util::fvector4(1,0,1,0))
	 * \endcode
	 * \param path the path to the property
	 * \param val the value to set of type T
	 * \returns a reference to the PropertyValue (this can be used later, e.g. if a vector is filled step by step
	 * the reference can be used to not ask for the Property each time)
	 */
	template<typename T> PropertyValue &setValueAs( const PropPath &path, const T &val ) {
		PropertyValue &ret = property( path );

		if( ret.isEmpty() ) { // set an empty property
			ret = val;
		} else if( ret.size() == 1 ) {
			if( ret.is<T>() ) { // override same type
				ret.castTo<T>() = val;
			} else {
				if( ret[0].apply( val ) ) {
					LOG( Debug, warning ) << "Storing " << MSubject( Value<T>( val ).toString( true ) ) << " as " << MSubject( ret.toString( true ) ) << " as old value was already stored in that type";
				} else {
					LOG( Runtime, error ) << "Property " << MSubject( path ) << " is already set to " << MSubject( ret.toString( true ) ) << " won't override with " << MSubject( Value<T>( val ).toString( true ) );
				}
			}
		} else
			LOG( Runtime, error ) << "Won't override multivalue property " << MSubject( path ) << " with " << MSubject( Value<T>( val ).toString( true ) );

		return ret;
	}
	/**
	 * Set the given property to a given value/type at a specified index.
	 * The needed flag (if set) will be kept.
	 * The property will be set to the given value at the given index if
	 * - the property is empty or
	 * - the index is beyond property the amount of stored values (the list will be filled up to the index)
	 * - the property already stores a value of the same type at the given index (value will be overwritten)
	 * - the property already stores a value at the given index and the new value can be converted to that type (value will be overwritten but type will be kept)
	 * The property will not be set and an error will be send to Runtime if
	 * - the property already stores a value at the given index and no conversion is available
	 * \code
	 * setValueAs("MyPropertyName", 5, isis::util::fvector4(1,0,1,0))
	 * \endcode
	 * \param path the path to the property
	 * \param at the index to store the value at
	 * \param val the value to set of type T
	 * \returns a reference to the PropertyValue (this can be used later, e.g. if a vector is filled step by step
	 * the reference can be used to not ask for the Property each time)
	 */
	template<typename T> PropertyValue &setValueAs( const PropPath &path, const T &val, size_t at ) {
		PropertyValue &ret = property( path );

		if( ret.size() <= at ) {
			LOG_IF(at, Debug, info ) << "Extending " << MSubject( std::make_pair( path, ret ) ) << " to fit length " << MSubject( at ); //dont tell about extending empty property
			ret.resize( at + 1, Value<T>( val ) ); //resizing will insert the value
		} else {
			if( ret.is<T>() ) { // overwrite same type
				ret[at].castTo<T>() = val;
			} else {
				if( ret[at].apply( val ) ) {
					LOG( Debug, warning ) << "Storing " << MSubject( Value<T>( val ).toString( true ) ) << " as " << MSubject( ret.toString( true ) ) << " as Property already has that type";
				} else {
					LOG( Runtime, error ) << "Property " << MSubject( path ) << " is already set to " << MSubject( ret.toString( true ) ) << " won't overwrite with " << MSubject( Value<T>( val ).toString( true ) );
				}
			}
		}

		return ret;
	}

	/**
	 * Request a property value via the given key in the given type.
	 * If the requested type is not equal to type the property is stored with, an automatic conversion is done.
	 * If that conversion failes an error is sent to Runtime.
	 * If there is no value yet T() is returned and a warning is sent to Runtime
	 * \code
	 * getValueAs<fvector4>( "MyPropertyName" );
	 * \endcode
	 * \param path the path to the property
	 * \param at index of the value to return 
	 * \returns the property with given type, if not set yet T() is returned.
	 */
	template<typename T> T getValueAs( const PropPath &path, size_t at )const {
		return getValueAsImpl<T>(path,boost::optional<size_t>(at));// uses single value ops, if at was not given
	}
	/**
	 * Request a property value via the given key in the given type.
	 * If the requested type is not equal to type the property is stored with, an automatic conversion is done.
	 * If that conversion failes an error is sent to Runtime.
	 * If there is no value yet T() is returned and a warning is sent to Runtime
	 * \note This is a single value operation. So warning is send to Debug, if accessing a multivalue property.
	 * \code
	 * getValueAs<fvector4>( "MyPropertyName" );
	 * \endcode
	 * \param path the path to the property
	 * \param at index of the value to return
	 * \returns the property with given type, if not set yet T() is returned.
	 */
	template<typename T> T getValueAs( const PropPath &path)const {
		return getValueAsImpl<T>(path,boost::optional<size_t>());
	}

	/**
	 * Get a valid reference to the stored value in a given type at a given index.
	 * This tries to access a property's stored value as reference.
	 * If the stored type is not T, a transformation is done in place.
	 * If that fails, false is returned.
	 * If the property does not exist (or does not store enough values) false will be returned as well
	 * \param path the path to the property
	 * \param at the index of the value to reference
	 * \returns boost::optional<T&> referencing the requested value or false
	 */
	template<typename T> boost::optional<T &> refValueAs( const PropPath &path, size_t at ) {
		return refValueAsImpl<T>(path, boost::optional<size_t>(at) );
	}
	/**
	 * Get a valid reference to the stored single value in a given type.
	 * This tries to access a property's stored value as reference.
	 * \note This is a single value operation. So warning is send to Debug, if accessing a multivalue property.
	 * If the stored type is not T, a transformation is done in place.
	 * If that fails, false is returned.
	 * If the property does not exist (or does not store enough values) false will be returned as well
	 * \param path the path to the property
	 * \returns boost::optional<T&> referencing the requested value or false
	 */
	template<typename T> boost::optional<T &> refValueAs( const PropPath &path) {
		return refValueAsImpl<T>(path, boost::optional<size_t>() );
	}
	/**
	 * Get a valid reference to the stored value in a given type.
	 * This tries to access a property's first stored value as reference.
	 * If the stored type is not T, a transformation is done in place.
	 * If that fails, false is returned.
	 * If the property does not exist (or is empty) it is created with def as first value.
	 * \param path the path to the property
	 * \param def the default value to be used when creating the property
	 * \returns boost::optional<T&> referencing the requested value or false
	 */
	template<typename T> boost::optional<T &> propertyValueAsOr( const PropPath &path, const T &def ) {
		boost::optional< PropertyValue & > fetched = tryFetchEntry<PropertyValue>( path );

		if( !fetched ) return boost::optional<T &>(); // return "false" in case of a failure

		if( fetched->isEmpty() ) { // we just created one, so set its value to def
			*fetched = def;
		} else if( !fetched->is<T>() ) { // apparently it already has a value so lets try use that
			if( !transform<T>( path, path ) ) {
				LOG( Runtime, warning ) << "Transforming Property " << path << " from " << util::MSubject( fetched->getTypeName() ) << " to "
										<< util::MSubject( util::Value<T>::staticName() ) << " failed";
				return boost::optional<T &>();
			}
		}

		assert( fetched->is<T>() );
		assert( !fetched->isEmpty() );
		return fetched->castTo<T>();
	}


	/**
	 * Request a property value via the given key in the given type.
	 * If the requested type is not equal to type the property is stored with, an automatic conversion is done.
	 * If that conversion failes an error is sent to Runtime.
	 * If there is no value yet def is returned silently
	 * \code
	 * getValueAs<fvector4>( "MyPropertyName" );
	 * \endcode
	 * \param path the path to the property
	 * \param def the value to be returned if the requested value does not exist
	 * \returns the property with given type, if not set yet def is returned.
	 */
	template<typename T> T getPropertyAsOr( const PropPath &path, const T &def )const {
		boost::optional< const PropertyValue & > ref = tryFindEntry<PropertyValue>( path );

		if( ref && !ref->isEmpty() )
			return ref->as<T>();
		else {
			return def;
		}
	}
	template<typename T> T getPropertyAsOr( const PropPath &path, size_t at, const T &def )const {
		const boost::optional< const PropertyValue & > ref = tryFindEntry<PropertyValue>( path );

		if( ref && ref->size() > at )
			return ref->at( at ).as<T>();
		else {
			return def;
		}
	}
	/**
	 * Rename a given property/branch.
	 * This is implemented as copy+delete and can also be used between branches.
	 * - if the target exist and overwrite is true a warning will be send, but it will still be overwritten
	 * - if the source does not exist a warning will be send and nothing is done
	 * \param oldname the path of the existing property to be moved
	 * \param newname the destinaton path of the move
	 * \param overwrite if true an existing target will be overwritten
	 * \returns true if renaming/moving was successful
	 */
	bool rename( const PropertyMap::PropPath& oldname, const PropertyMap::PropPath& newname, bool overwrite=false );
	///////////////////////////////////////////////////////////////////////////
	// extended tools
	///////////////////////////////////////////////////////////////////////////

	/// \returns a flat representation of the whole property tree
	FlatMap getFlatMap( )const;

	/**
	 * Try reading a JSON tree into the PropertyMap.
	 * \param streamBegin pointer to the begin of the character stream
	 * \param streamEnd pointer behind the end of the character stream
	 * \param extra_token to seperate entry paths in the map ("CsaSeries.MrPhoenixProtocol" from json will become property path "CsaSeries/MrPhoenixProtocol" if '.' is given)
	 * \param list_trees colon separated labels for json-subtrees where property listing should be tried before using lists from the known types (e.g. "samples:slices")
	 * \returns true if the whole stream was parsed
	 */
	bool readJson( uint8_t* streamBegin, uint8_t* streamEnd, char extra_token, std::string list_trees=std::string() );

	/**
	 * "Print" the property tree.
	 * Will send the name and the result of \link PropertyValue::toString \endlink(label) to the given ostream.
	 * Is equivalent to common streaming operation but has the option to print the type of the printed properties.
	 * \param out the output stream to use
	 * \param label print the type of the property (see Value::toString())
	 */
	std::ostream &print( std::ostream &out, bool label = false )const;
};

}
}

namespace std
{
/// Streaming output for PropertyMap::PropPath
template<typename charT, typename traits>
basic_ostream<charT, traits>& operator<<( basic_ostream<charT, traits> &out, const isis::util::PropertyMap::PropPath &s )
{
	isis::util::listToOStream( s.begin(), s.end(), out, std::string( 1, s.pathSeperator ), "", "" );
	return out;
}
/// Streaming output for PropertyMap
template<typename charT, typename traits>
basic_ostream<charT, traits>& operator<<( basic_ostream<charT, traits> &out, const isis::util::PropertyMap &s )
{
	const isis::util::PropertyMap::FlatMap buff = s.getFlatMap();
	isis::util::listToOStream( buff.begin(), buff.end(), out );
	return out;
}
}

#endif
