#pragma once

#include "arraytypes.hpp"
#include "color.hpp"
#include "value.hpp"

namespace isis::data{

typedef std::pair<util::ValueNew,util::ValueNew> scaling_pair;
enum autoscaleOption {noscale, autoscale, noupscale, upscale};
class ValueArrayNew;

namespace _internal{
	
/// @cond _internal
template<typename T, uint8_t STEPSIZE> std::pair<T, T> calcMinMax( const T *data, size_t len )
{
	static_assert( std::numeric_limits<T>::has_denorm != std::denorm_indeterminate, "denormisation not known" ); //well we're pretty f**ed in this case
	std::pair<T, T> result(
		std::numeric_limits<T>::max(),
		std::numeric_limits<T>::has_denorm ? -std::numeric_limits<T>::max() : std::numeric_limits<T>::min() //for types with denormalization min is _not_ the lowest value
	);
	LOG( Runtime, verbose_info ) << "using generic min/max computation for " << util::ValueNew::staticName<T>();

	for ( const T *i = data; i < data + len; i += STEPSIZE ) {
		if(
			std::numeric_limits<T>::has_infinity &&
			( *i == std::numeric_limits<T>::infinity() || *i == -std::numeric_limits<T>::infinity() )
		)
			continue; // skip this one if its inf

		if(std::isnan(*i))
			continue; // skip this one if its NaN
			
		if ( *i > result.second )result.second = *i; //*i is the new max if its bigger than the current (gets rid of nan as well)

		if ( *i < result.first )result.first = *i; //*i is the new min if its smaller than the current (gets rid of nan as well)
	}

	LOG_IF(std::numeric_limits<T>::has_infinity && result.first>result.second,Runtime,warning) 
		<< "Skipped all elements of this array, as they all are inf. Results will be invalid.";

	return result;
}

#ifdef __SSE2__
////////////////////////////////////////////////
// specialize calcMinMax for (u)int(8,16,32)_t /
////////////////////////////////////////////////

template<> std::pair< uint8_t,  uint8_t> calcMinMax< uint8_t, 1>( const  uint8_t *data, size_t len );
template<> std::pair<uint16_t, uint16_t> calcMinMax<uint16_t, 1>( const uint16_t *data, size_t len );
template<> std::pair<uint32_t, uint32_t> calcMinMax<uint32_t, 1>( const uint32_t *data, size_t len );

template<> std::pair< int8_t,  int8_t> calcMinMax< int8_t, 1>( const  int8_t *data, size_t len );
template<> std::pair<int16_t, int16_t> calcMinMax<int16_t, 1>( const int16_t *data, size_t len );
template<> std::pair<int32_t, int32_t> calcMinMax<int32_t, 1>( const int32_t *data, size_t len );
#endif //__SSE2__

API_EXCLUDE_BEGIN;
struct getMinMaxVisitor { 
	std::pair<util::ValueNew,util::ValueNew> minmax;
	// fallback for unsupported types
	template<typename T> void operator()( const std::shared_ptr<T> &/*ref*/ ) const {
		LOG( Debug, error ) << "min/max computation of " << util::ValueNew::staticName<T>() << " is not supported";
	}
	// generic min-max for numbers 
	template<typename T> std::enable_if<std::is_arithmetic<T>::value> operator()( const std::shared_ptr<T> &ref ) const {
		#pragma warning test me
		minmax=calcMinMax<T, 1>( &ref[0], ref.getLength() );
	}
	//specialization for color
	template<typename T> std::enable_if<std::is_arithmetic<T>::value> operator()( const std::shared_ptr<util::color<T> > &ref ) const {
		std::pair<util::color<T> , util::color<T> > ret;

		for( uint_fast8_t i = 0; i < 3; i++ ) {
			const std::pair<T, T> buff = calcMinMax<T, 3>( &ref[0].r + i, ref.getLength() * 3 );
			*( &ret.first.r + i ) = buff.first;
			*( &ret.second.r + i ) = buff.second;
		}
		#pragma warning test me
		minmax=ret;
	}
	//specialization for complex
	template<typename T> std::enable_if<std::is_arithmetic<T>::value> operator()( const std::shared_ptr<std::complex<T> > &ref ) const {
		//use compute min/max of magnitute / phase
		auto any_nan= [](const std::complex<T> &v){return std::isnan(v.real()) || std::isnan(v.imag());};
		const auto start=std::find_if_not(std::begin(ref),std::end(ref),any_nan);
		if(start==std::end(ref)){
			LOG(Runtime,error) << "Array is all NaN, returning NaN/NaN as minimum/maximum";
			return std::make_pair(std::numeric_limits<T>::quiet_NaN(),std::numeric_limits<T>::quiet_NaN());
		} else {
			T ret_min_sqmag=std::norm(*start),ret_max_sqmag=std::norm(*start);
			
			for(auto i=start;i!=std::end(ref);i++){
				if(!any_nan(*i)){
					const T &sqmag=std::norm(*i);
					if(ret_min_sqmag>sqmag)ret_min_sqmag=sqmag;
					if(ret_max_sqmag<sqmag)ret_max_sqmag=sqmag;
				}
			}
			#pragma warning test me
			minmax=std::make_pair(std::sqrt(ret_min_sqmag),std::sqrt(ret_max_sqmag));
		}
	}
};

/// @endcond
API_EXCLUDE_END;

/// Proxy-Deleter to encapsulate the real deleter/shared_ptr when creating shared_ptr for parts of a shared_ptr
class DelProxy : public std::shared_ptr<const void>
{
public:
    /**
        * Create a proxy for a given master shared_ptr
        * This increments the use_count of the master and thus keeps the
        * master from being deleted while parts of it are still in use.
        */
    DelProxy( const ValueArrayNew &master );
    /// decrement the use_count of the master when a specific part is not referenced anymore
    void operator()( const void *at );
};

class ConstValueAdapter
{
	template<bool BB> friend class GenericValueIterator; //allow the iterators (and only them) to create the adapter
public:
	typedef const util::ValueNew ( *Getter )( const void * );
	typedef void ( *Setter )( void *, const util::ValueNew & );
protected:
	const Getter getter;
	const uint8_t *const p;
	ConstValueAdapter( const uint8_t *const _p, Getter _getValueFunc );
public:
	// to make some algorithms work
	bool operator==( const util::ValueNew &val )const;
	bool operator!=( const util::ValueNew &val )const;

	bool operator<( const util::ValueNew &val )const;
	bool operator>( const util::ValueNew &val )const;

	operator const util::ValueNew()const;
	const util::ValueNew operator->() const;
	const std::string toString( bool label = false )const;
};
class WritingValueAdapter: public ConstValueAdapter
{
	Setter setValueFunc;
	size_t byteSize;
	template<bool BB> friend class GenericValueIterator; //allow the iterators (and only them) to create the adapter
protected:
	WritingValueAdapter( uint8_t *const _p, Getter _getValueFunc, Setter _setValueFunc, size_t _size );
public:
	WritingValueAdapter operator=( const util::ValueNew &val );
	void swapwith( const WritingValueAdapter &b )const; // the WritingValueAdapter is const not what its dereferencing
};

template<bool IS_CONST> class GenericValueIterator :
	public std::iterator < std::random_access_iterator_tag,
	typename std::conditional<IS_CONST, ConstValueAdapter, WritingValueAdapter>::type,
	ptrdiff_t,
	typename std::conditional<IS_CONST, ConstValueAdapter, WritingValueAdapter>::type,
	typename std::conditional<IS_CONST, ConstValueAdapter, WritingValueAdapter>::type
	>
{
	typedef typename std::conditional<IS_CONST, const uint8_t *, uint8_t *>::type ptr_type;
	ptr_type p, start; //we need the starting position for operator[]
	size_t byteSize;
	ConstValueAdapter::Getter getValueFunc;
	ConstValueAdapter::Setter setValueFunc;
	friend class GenericValueIterator<true>; //yes, I'm my own friend, sometimes :-) (enables the constructor below)
public:
	GenericValueIterator( const GenericValueIterator<false> &src ): //will become additional constructor from non const if this is const, otherwise overrride the default copy contructor
		p( src.p ), start( src.p ), byteSize( src.byteSize ), getValueFunc( src.getValueFunc ), setValueFunc( src.setValueFunc ) {}
	GenericValueIterator(): p( NULL ), start( p ), byteSize( 0 ), getValueFunc( NULL ), setValueFunc( NULL ) {}
	GenericValueIterator( ptr_type _p, ptr_type _start, size_t _byteSize, ConstValueAdapter::Getter _getValueFunc, ConstValueAdapter::Setter _setValueFunc ):
		p( _p ), start( _start ), byteSize( _byteSize ), getValueFunc( _getValueFunc ), setValueFunc( _setValueFunc )
	{}

	GenericValueIterator<IS_CONST>& operator++() {p += byteSize; return *this;}
	GenericValueIterator<IS_CONST>& operator--() {p -= byteSize; return *this;}

	GenericValueIterator<IS_CONST> operator++( int ) {GenericValueIterator<IS_CONST> tmp = *this; ++*this; return tmp;}
	GenericValueIterator<IS_CONST> operator--( int ) {GenericValueIterator<IS_CONST> tmp = *this; --*this; return tmp;}

	typename GenericValueIterator<IS_CONST>::reference operator*() const;
	typename GenericValueIterator<IS_CONST>::pointer  operator->() const {return operator*();}

	bool operator==( const GenericValueIterator<IS_CONST>& cmp )const {return p == cmp.p;}
	bool operator!=( const GenericValueIterator<IS_CONST>& cmp )const {return !( *this == cmp );}

	bool operator>( const GenericValueIterator<IS_CONST> &cmp )const {return p > cmp.p;}
	bool operator<( const GenericValueIterator<IS_CONST> &cmp )const {return p < cmp.p;}

	bool operator>=( const GenericValueIterator<IS_CONST> &cmp )const {return p >= cmp.p;}
	bool operator<=( const GenericValueIterator<IS_CONST> &cmp )const {return p <= cmp.p;}

	typename GenericValueIterator<IS_CONST>::difference_type operator-( const GenericValueIterator<IS_CONST> &cmp )const {return ( p - cmp.p ) / byteSize;}

	GenericValueIterator<IS_CONST> operator+( typename GenericValueIterator<IS_CONST>::difference_type n )const
	{return ( GenericValueIterator<IS_CONST>( *this ) += n );}
	GenericValueIterator<IS_CONST> operator-( typename GenericValueIterator<IS_CONST>::difference_type n )const
	{return ( GenericValueIterator<IS_CONST>( *this ) -= n );}


	GenericValueIterator<IS_CONST> &operator+=( typename GenericValueIterator<IS_CONST>::difference_type n )
	{p += ( n * byteSize ); return *this;}
	GenericValueIterator<IS_CONST> &operator-=( typename GenericValueIterator<IS_CONST>::difference_type n )
	{p -= ( n * byteSize ); return *this;}

	typename GenericValueIterator<IS_CONST>::reference operator[]( typename GenericValueIterator<IS_CONST>::difference_type n )const {
		//the book says it has to be the n-th elements of the whole object, so we have to start from what is hopefully the beginning
		return *( GenericValueIterator<IS_CONST>( start, start, byteSize, getValueFunc, setValueFunc ) += n );
	}

};
//specialise the dereferencing operators. They have to return (and create) different objects with different constructors
template<> GenericValueIterator<true>::reference GenericValueIterator<true>::operator*() const;
template<> GenericValueIterator<false>::reference GenericValueIterator<false>::operator*() const;


/**
 * Basic iterator for ValueArray.
 * This is a common iterator following the random access iterator model.
 * It is not part of the reference counting used in ValueArray. So make sure you keep the ValueArray you created it from while you use this iterator.
 */
template<typename TYPE> class TypedArrayIterator: public std::iterator<std::random_access_iterator_tag, TYPE>
{
    TYPE *p;
    typedef typename std::iterator<std::random_access_iterator_tag, TYPE>::difference_type distance;
public:
    TypedArrayIterator(): p( NULL ) {}
    TypedArrayIterator( TYPE *_p ): p( _p ) {}
    TypedArrayIterator( const TypedArrayIterator<typename std::remove_const<TYPE>::type > &src ): p( src.p ) {}

    TypedArrayIterator<TYPE>& operator++() {++p; return *this;}
    TypedArrayIterator<TYPE>& operator--() {--p; return *this;}

    TypedArrayIterator<TYPE>  operator++( int ) {TypedArrayIterator<TYPE> tmp = *this; ++*this; return tmp;}
    TypedArrayIterator<TYPE>  operator--( int ) {TypedArrayIterator<TYPE> tmp = *this; --*this; return tmp;}

    TYPE &operator*() const { return *p; }
    TYPE *operator->() const { return p; }

    bool operator==( const TypedArrayIterator<TYPE> &cmp )const {return p == cmp.p;}
    bool operator!=( const TypedArrayIterator<TYPE> &cmp )const {return !( *this == cmp );}

    bool operator>( const TypedArrayIterator<TYPE> &cmp )const {return p > cmp.p;}
    bool operator<( const TypedArrayIterator<TYPE> &cmp )const {return p < cmp.p;}

    bool operator>=( const TypedArrayIterator<TYPE> &cmp )const {return p >= cmp.p;}
    bool operator<=( const TypedArrayIterator<TYPE> &cmp )const {return p <= cmp.p;}

    TypedArrayIterator<TYPE> operator+( distance n )const {return TypedArrayIterator<TYPE>( p + n );}
    TypedArrayIterator<TYPE> operator-( distance n )const {return TypedArrayIterator<TYPE>( p - n );}

    distance operator-( const TypedArrayIterator<TYPE> &cmp )const {return p - cmp.p;}

    TypedArrayIterator<TYPE> &operator+=( distance n ) {p += n; return *this;}
    TypedArrayIterator<TYPE> &operator-=( distance n ) {p -= n; return *this;}

    TYPE &operator[]( distance n )const {return *( p + n );}

    operator TYPE*(){return p;}
};

}
	
/**
 * Generic class for type (and length) - aware pointers.
 * The class is designed for arrays, but you can also "point" to an single element
 * by just use "1" for the length.
 * The pointers are reference counted and will be deleted automatically by a customizable deleter.
 * The copy is cheap, thus the copy of a ValueArray will reference the same data.
 * The usual pointer dereferencing interface ("*", "->" and "[]") is supported.
 */
class ValueArrayNew: protected ArrayTypes
{
	size_t m_length;
// 	scaling_pair getScaling( const scaling_pair &scale, unsigned short ID )const;
	/// delete-functor which does nothing (in case someone else manages the data).
	struct NonDeleter {
		template<typename T> void operator()( T *p ) {
			//we have to cast the pointer to void* here, because in case of uint8_t it will try to print the "string"
			LOG( Debug, verbose_info ) << "Not freeing pointer " << ( void * )p << " (" << ValueArrayNew::staticName<T>() << ") as automatic deletion was disabled for it";
		};
	};
	/// Default delete-functor for c-arrays (uses free()).
	struct BasicDeleter {
		template<typename T> void operator()( T *p ) {
			//we have to cast the pointer to void* here, because in case of uint8_t it will try to print the "string"
			LOG( Debug, verbose_info ) << "Freeing pointer " << ( void * )p << " (" << ValueArrayNew::staticName<T>() << ") ";
			free( p );
		};
	};
	template<typename T> static const util::ValueNew getValueFrom( const void *p ) {
		return util::ValueNew( *reinterpret_cast<const TYPE *>( p ) );
	}
	template<typename T> static void setValueInto( void *p, const util::ValueNew &val ) {
		*reinterpret_cast<TYPE *>( p ) = val.as<TYPE>();
	}

public:
	typedef _internal::GenericValueIterator<false> value_iterator;
	typedef _internal::GenericValueIterator<true> const_value_iterator;
	typedef value_iterator value_reference;
	typedef const_value_iterator const_value_reference;

	/**
	 * Creates ValueArray from a std::shared_ptr of the same type.
	 * It will inherit the deleter of the shared_ptr.
	 * \param ptr the shared_ptr to share the data with
	 * \param length the length of the used array (ValueArray does NOT check for length,
	 * this is just here for child classes which may want to check)
	 */
	template<typename T> ValueArrayNew( const std::shared_ptr<T> &ptr, size_t length ): ArrayTypes( ptr ), m_length(length) {
		static_assert(!std::is_const<T>::value,"ValueArray type must not be const");
		LOG_IF( length == 0, Debug, warning )
            << "Creating an empty (lenght==0) ValueArray of type " << util::MSubject( staticName<T>() )
			<< " you should overwrite it with a useful pointer before using it";
		LOG_IF(length != 0 && !ptr,Debug,error) << "Creating invalid ValueArray from null pointer";
	}

	/**
	 * Creates ValueArray from a pointer of type TYPE.
	 * The pointers are automatically deleted by an instance of BasicDeleter and should not be used outside once used here.
	 * \param ptr the pointer to the used array
	 * \param length the length of the used array (ValueArray does NOT check for length,
	 * this is just here for child classes which may want to check)
	 */
	template<typename T> ValueArrayNew( T *const ptr, size_t length ): ValueArrayNew(std::shared_ptr<T>(ptr),length) {}

	/**
	 * Creates a ValueArray pointing to a newly allocated array of elements of the given type.
	 * The array is zero-initialized.
	 * If the requested length is 0 no memory will be allocated and the pointer will be "empty".
	 * \param length amount of elements in the new array
	 */
	template<typename T> ValueArrayNew( size_t length ): ValueArrayNew(( T * )calloc( length, sizeof( T ) ),  length ) {}

	/// \return true if the stored type is T
	template<typename T> bool is()const{
		return std::holds_alternative<std::shared_ptr<T>>(*this);
	}

    std::shared_ptr<const void> getRawAddress( size_t offset = 0 )const;

	/// \returns the length (in elements) of the data pointed to
	size_t getLength()const;

    template<typename T> static std::string staticName(){
        return ValueNew(T()).typeName();
    }
    template<typename T> static std::string staticIndex(){
        return ValueNew(T()).index();
    }
    std::string typeName()const;

	/**
	 * Splice up the ValueArray into equal sized blocks.
	 * This virtually creates new data blocks of the given size by computing new pointers into the block and creating ValueArray objects for them.
	 * This ValueArray objects use the reference counting of the original ValueArray via DelProxy, so the original data are only deleted (as a whole)
	 * when all spliced and all "normal" ValueArray for this data are deleted.
	 * \param size the maximum size of the spliced parts of the data (the last part can be smaller)
	 * \returns a vector of references to ValueArray's which point to the parts of the spliced data
	 */
    virtual std::vector<ValueArrayNew> splice( size_t size );

	///get the scaling (and offset) which would be used in an conversion
    virtual scaling_pair getScalingTo( unsigned short typeID, autoscaleOption scaleopt = autoscale );
	virtual scaling_pair getScalingTo( unsigned short typeID, const std::pair<util::ValueNew, util::ValueNew> &minmax, autoscaleOption scaleopt = autoscale )const;

	/**
	 * Create new data in memory containg a (converted) copy of this.
	 * Allocates new memory of the requested type and copies the (converted) content of this into that memory.
	 * \param ID the ID of the type the new ValueArray (referenced by the ValueArrayNew returned) should have (if not given, type of the source is used)
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	ValueArrayNew copyByID( unsigned short ID = 0, scaling_pair scaling = scaling_pair() ) const;

	/**
	 * Copies elements from this into another ValueArray.
	 * This is always a deep copy, regardless of the types.
	 * If necessary, a conversion will be done.
	 * If the this and the target are not of the same length:
	 * - the shorter length will be used
	 * - a warning about it will be sent to Debug
	 * \param dst the ValueArray-object to copy into
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	bool copyTo( ValueArrayNew &dst, scaling_pair scaling = scaling_pair() )const;

	/**
	 * Copies elements from this into raw memory.
	 * This is allways a deep copy, regardless of the types.
	 * If the this and the target are not of the same length:
	 * - the shorter length will be used
	 * - a warning about it will be sent to Debug
	 * \param dst pointer to the target memory
	 * \param len size (in elements) of the target memory
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	template<typename T> bool copyToMem( T *dst, size_t len, scaling_pair scaling = scaling_pair() )const {
		ValueArrayNew cont( dst, len, typename ValueArrayNew::NonDeleter() );
		return copyTo( cont, scaling );
	}

	/**
	 * Copies elements from raw memory into  this.
	 * This is allways a deep copy, regardless of the types.
	 * If the this and the target are not of the same length:
	 * - the shorter length will be used
	 * - a warning about it will be sent to Debug
	 * \param src pointer to the target memory
	 * \param len size (in elements) of the target memory
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	template<typename T> bool copyFromMem( const T *const src, size_t len, scaling_pair scaling = scaling_pair() ) {
		ValueArrayNew cont( const_cast<T *>( src ), len, typename ValueArrayNew::NonDeleter() ); //its ok - we're no going to change it
		return cont.copyTo( *this, scaling );
	}

	/**
	 * Create a ValueArray of given type and length.
	 * This allocates memory as needed but does not initialize it.
	 * \returns a Reference to a ValueArray pointing to the allocated memory. Or an empty Reference if the creation failed.
	 */
	static ValueArrayNew createByID( unsigned short ID, size_t len );

	/**
	 * Copy this to a new ValueArray\<T\> using newly allocated memory.
	 * This will create a new ValueArray of type T and the length of this.
	 * The memory will be allocated and the data of this will be copy-converted to T using min/max as value range.
	 * If the conversion fails, an error will be send to CoreLog and the data of the newly created ValueArray will be undefined.
	 * \returns a the newly created ValueArray
	 */
    template<typename T> ValueArrayNew copyAs( scaling_pair scaling = scaling_pair() )const {
        return copyByID( staticIndex<T>(), scaling );;
	}

	/**
	 * Get this as a ValueArray of a specific type.
	 * This does an automatic conversion into a new ValueArray if one of following is true:
	 * - the target type is not the current type
	 * - scaling.first (the scaling factor) is not 1
	 * - scaling.first (the scaling offset) is not 0
	 *
	 * Otherwise a cheap copy is done.
	 * \param ID the ID of the requeseted type (use ValueArray::staticID())
	 * \param scaling the scaling to be used (determined automatically if not given)
	 * \returns a reference of eigther a cheap copy or a newly created ValueArray
	 * \returns an empty reference if the conversion failed
	 */
	ValueArrayNew  convertByID( unsigned short ID, scaling_pair scaling = scaling_pair() );


	/**
	 * Get this as a ValueArray of a specific type.
	 * This does an automatic conversion into a new ValueArray if one of following is true:
	 * - the target type is not the current type
	 * - scaling.first (the scaling factor) is not 1
	 * - scaling.first (the scaling offset) is not 0
	 *
	 * Otherwise a cheap copy is done.
	 * \param scaling the scaling to be used (determined automatically if not given)
	 * \returns eigther a cheap copy or a newly created ValueArray
	 */
    template<typename T> ValueArrayNew as( scaling_pair scaling = scaling_pair() ) {
        return convertByID( staticIndex<T>(), scaling );
	}

	/**
	 * Create a new ValueArray, of the same type, but differnent size in memory.
	 * (The actual data are _not_ copied)
	 * \param length length of the new memory block in elements of the given TYPE
	 */
	ValueArrayNew cloneToNew( size_t length )const;

	/// \returns the byte-size of the type of the data this ValueArray points to.
	size_t bytesPerElem()const;

	/**
	 * Copy a range of elements to another ValueArray of the same type.
	 * \param start first element in this to be copied
	 * \param end last element in this to be copied
	 * \param dst target for the copy
	 * \param dst_start starting element in dst to be overwritten
	 */
	void copyRange( size_t start, size_t end, ValueArrayNew &dst, size_t dst_start )const;

	/// \returns the number of references using the same memory as this.
	size_t useCount()const;

	/**
	 * Get minimum/maximum of a ValueArray.
	 * This computes the minimum and maximum value of the stored data and stores them in ValueReference-Objects.
	 * This actually returns the bounding box of the values in the value space of the type. This means:
	 * - min/max numbers for numbers from a 1-D value space (aka real numbers)
	 * - complex(lowest real value,lowest imaginary value) / complex(biggest real value,biggest imaginary value) for complex numbers
	 * - color(lowest red value,lowest green value, lowest blue value)/color(biggest red value,biggest green value, biggest blue value) for color
	 * The computed min/max are of the same type as the stored data, but can be compared to other ValueReference without knowing this type via the lt/gt function of ValueBase.
	 * The following code checks if the value range of ValueArray-object data1 is a real subset of data2:
	 * \code
	 * std::pair<util::ValueNew,util::ValueNew> minmax1=data1.getMinMax(), minmax2=data2.getMinMax();
	 * if(minmax1.first->gt(minmax2.second) && minmax1.second->lt(minmax2.second)
	 *  std::cout << minmax1 << " is a subset of " minmax2 << std::endl;
	 * \endcode
	 * \returns a pair of ValueReferences referring to the found minimum/maximum of the data
	 */
	std::pair<util::ValueNew, util::ValueNew> getMinMax()const;

	/**
	 * Compare the data of two ValueArray.
	 * Counts how many elements in this and the given ValueArray are different within the given range.
	 * If the type of this is not equal to the type of the given ValueArray the whole length is assumed to be different.
	 * If the given range does not fit into this or the given ValueArray an error is send to the runtime log and the function will probably crash.
	 * \param start the first element in this, which schould be compared to the first element in the given TyprPtr
	 * \param end the first element in this, which should _not_ be compared anymore to the given TyprPtr
	 * \param dst the given ValueArray this should be compared to
	 * \param dst_start the first element in the given TyprPtr, which schould be compared to the first element in this
	 * \returns the amount of elements which actually differ in both ValueArray or the whole length of the range when the types are not equal.
	 */
	size_t compare( size_t start, size_t end, const ValueArrayNew &dst, size_t dst_start )const;

	bool isValid();

    std::shared_ptr<void> getRawAddress( size_t offset = 0 );
    std::shared_ptr<const void> getRawAddress( size_t offset = 0 )const;
    value_iterator beginGeneric();
    const_value_iterator beginGeneric()const;
    value_iterator endGeneric();
    const_value_iterator endGeneric()const;


	/// @copydoc util::Value::toString
    std::string toString( bool labeled = false )const;

	bool isFloat() const;
	bool isInteger() const;

    std::vector<ValueArrayNew> splice(size_t size )const;
	//
    scaling_pair getScalingTo( unsigned short typeID, autoscaleOption scaleopt = autoscale )const;
    void endianSwap();
};

}
