#pragma once

#include <utility>
#include <ostream>

#include "types_array.hpp"
#include "color.hpp"
#include "value.hpp"
#include "valuearray_converter.hpp"
#include "valuearray_minmax.hpp"
#include "valuearray_iterator.hpp"

namespace isis::data{

struct scaling_pair {
	scaling_pair()=default;
	[[nodiscard]] bool isRelevant()const{return valid && !(scale==1 && offset==0);}
	scaling_pair(util::Value _scale, util::Value _offset): scale(std::move(_scale)), offset(std::move(_offset)), valid(true){}
	util::Value scale;
	util::Value offset;
	bool valid=false;
	friend std::ostream &operator<<(std::ostream &os, const scaling_pair &pair);
};

class ValueArray;

namespace _internal{

/// Proxy-Deleter to encapsulate the real deleter/shared_ptr when creating shared_ptr for parts of a shared_ptr
class DelProxy : public std::shared_ptr<const void>
{
public:
	/**
	 * Create a proxy for a given master shared_ptr
	 * This increments the use_count of the master and thus keeps the
	 * master from being deleted while parts of it are still in use.
	 */
	explicit DelProxy( const ValueArray &master );
	/// decrement the use_count of the master when a specific part is not referenced anymore
	void operator()( const void *at );
};

}

/**
 * Generic class for type (and length) - aware pointers.
 * The class is designed for arrays, but you can also "point" to an single element
 * by just use "1" for the length.
 * The pointers are reference counted and will be deleted automatically by a customizable deleter.
 * The copy is cheap, thus the copy of a ValueArray will reference the same data.
 * The usual pointer de-referencing interface ("*", "->" and "[]") is supported.
 */
class ValueArray: protected ArrayTypes
{
	size_t m_length;
	/// Default delete-functor for c-arrays (uses free()).
	struct BasicDeleter {
		template<typename T> void operator()( T *p )const {
			//we have to cast the pointer to void* here, because in case of uint8_t it will try to print the "string"
			LOG( Debug, verbose_info ) << "Freeing pointer " << ( void * )p << " (" << util::typeName<T>() << ") ";
			free( p );
		};
	};
	template<typename TYPE> static const util::Value getValueFrom(const void *p ) {
		return util::Value(*reinterpret_cast<const TYPE *>( p ) );
	}
	template<typename TYPE> static void setValueInto( void *p, const util::Value &val ) {
		*reinterpret_cast<TYPE *>( p ) = val.as<TYPE>();
	}

	static const _internal::ValueArrayConverterMap &converters();
protected:
	template<typename T> constexpr static void checkType(){
		constexpr auto id=util::_internal::variant_index<ArrayTypes,std::shared_ptr<std::remove_cv_t<T>>>();
		static_assert(id!=std::variant_npos,"invalid array type");
	}
	/**
	* Helper to sanitise scaling.
	* \retval scaling if !(scaling.first.isEmpty() || scaling.second.isEmpty())
	* \retval 1/0 if current type is equal to the requested type
	* \retval ValueArrayBase::getScalingTo otherwise
	*/
	[[nodiscard]] scaling_pair getScaling( const scaling_pair &scaling, short unsigned int ID )const
	{
		if( scaling.valid )
			return scaling;//we already have a valid scaling, use that
		else{ //validate scaling
			isis::data::scaling_pair computed_scaling=getScalingTo( ID );
			if(!computed_scaling.valid)
				throw(std::logic_error("Invalid scaling"));
			LOG_IF( computed_scaling.scale.lt(1), Runtime, warning ) << "Downscaling your values by Factor " << computed_scaling.scale.as<double>() << " you might lose information.";
			return computed_scaling;
		} 
	}

public:
	template<int I> using TypeByIndex = typename std::variant_alternative<I, ArrayTypes>::type;

	using Converter = _internal::ValueArrayConverterMap::mapped_type::mapped_type;
	using iterator =  _internal::GenericValueIterator<false>;
	using const_iterator = _internal::GenericValueIterator<true>;
	using reference = iterator::reference;
	using const_reference = const_iterator::reference;

	ValueArray();//creates an invalid value array
	/**
	 * Creates ValueArray from a std::shared_ptr of the same type.
	 * It will inherit the deleter of the shared_ptr.
	 * \param ptr the shared_ptr to share the data with
	 * \param length the length of the used array (ValueArray does NOT check for length,
	 * this is just here for child classes which may want to check)
	 */
	template<typename T> ValueArray(const std::shared_ptr<T> &ptr, size_t length ): ArrayTypes(ptr ), m_length(length) {
		checkType<T>();
		static_assert(!std::is_const<T>::value,"ValueArray type must not be const");
		assert(beginTyped<T>()==ptr.get());
	}

	/**
	 * Creates ValueArray from a pointer of type TYPE.
	 * The pointers are automatically deleted by an instance of BasicDeleter and should not be used outside once used here.
	 * \param ptr the pointer to the used array
	 * \param length the length of the used array (ValueArray does NOT check for length,
	 * this is just here for child classes which may want to check)
	 */
	template<typename T, typename DELETER=ValueArray::BasicDeleter>
	ValueArray(T *const ptr, size_t length, const DELETER &deleter=DELETER())	: ValueArray(std::shared_ptr<T>(ptr, deleter), length) {}

	/**
	 * Creates a ValueArray pointing to a newly allocated array of elements of the given type.
	 * The array is zero-initialized.
	 * If the requested length is 0 no memory will be allocated and the pointer will be "empty".
	 * \param length amount of elements in the new array
	 */
	template<typename T, typename DELETER=ValueArray::BasicDeleter>
	static ValueArray make(size_t length, const DELETER &deleter=DELETER() ){
		return ValueArray(( T * )calloc(length, sizeof( T ) ), length, deleter );
	} //@todo maybe make it TypedArray
	
	template<typename VIS> decltype(auto) visit(VIS&& visitor)
	{
		return std::visit(std::forward<VIS>(visitor),static_cast<ArrayTypes&>(*this));
	}
	template<typename VIS> decltype(auto) visit(VIS&& visitor)const
	{
		return std::visit(std::forward<VIS>(visitor),static_cast<const ArrayTypes&>(*this));
	}


	/// \return true if the stored type is T
	template<typename T> [[nodiscard]] bool is()const{
		checkType<T>();
		return std::holds_alternative<std::shared_ptr<T>>(*this);
	}

	[[nodiscard]] const Converter& getConverterTo( unsigned short ID )const;
	static const Converter& getConverterFromTo( unsigned short fromID, unsigned short toID );

	/// \returns the length (in elements) of the data pointed to
	[[nodiscard]] size_t getLength()const;

	[[nodiscard]] std::string typeName()const;

	[[nodiscard]] size_t getTypeID()const;

	/**
	 * Splice up the ValueArray into equal sized blocks.
	 * This virtually creates new data blocks of the given size by computing new pointers into the block and creating ValueArray objects for them.
	 * This ValueArray objects use the reference counting of the original ValueArray via DelProxy, so the original data are only deleted (as a whole)
	 * when all spliced and all "normal" ValueArray for this data are deleted.
	 * \param size the maximum size of the spliced parts of the data (the last part can be smaller)
	 * \returns a vector of references to ValueArray's which point to the parts of the spliced data
	 */
	[[nodiscard]] virtual std::vector<ValueArray> splice(size_t size )const;

	///get the scaling (and offset) which would be used in an conversion
	[[nodiscard]] virtual scaling_pair getScalingTo( unsigned short typeID )const;
	[[nodiscard]] virtual scaling_pair getScalingTo( unsigned short typeID, const std::pair<util::Value, util::Value> &minmax )const;

	/**
	 * Create new data in memory containing a (converted) copy of this.
	 * Allocates new memory of the requested type and copies the (converted) content of this into that memory.
	 * \param ID the ID of the type the new ValueArray (referenced by the ValueArray returned) should have (if not given, type of the source is used)
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	[[nodiscard]] ValueArray copyByID(size_t ID, const scaling_pair &scaling ) const;

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
	bool copyTo(ValueArray &dst, scaling_pair scaling = scaling_pair() )const;

	/**
	 * Copies elements from this into raw memory.
	 * This is always a deep copy, regardless of the types.
	 * If the this and the target are not of the same length:
	 * - the shorter length will be used
	 * - a warning about it will be sent to Debug
	 * \param dst pointer to the target memory
	 * \param len size (in elements) of the target memory
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	template<typename T> bool copyToMem( T *dst, size_t len, scaling_pair scaling = scaling_pair() )const {
		ValueArray cont(dst, len, NonDeleter() );
		return copyTo( cont, std::move(scaling) );
	}

	/**
	 * Copies elements from raw memory into  this.
	 * This is always a deep copy, regardless of the types.
	 * If the this and the target are not of the same length:
	 * - the shorter length will be used
	 * - a warning about it will be sent to Debug
	 * \param src pointer to the target memory
	 * \param len size (in elements) of the target memory
	 * \param scaling the scaling to be used if a conversion is necessary (computed automatically if not given)
	 */
	template<typename T> bool copyFromMem( const T *const src, size_t len, scaling_pair scaling = scaling_pair() ) {
		ValueArray cont(const_cast<T *>( src ), len, NonDeleter() ); //it's ok - we're not going to change it
		return cont.copyTo( *this, std::move(scaling) );
	}

	/**
	 * Create a ValueArray of given type and length.
	 * This allocates memory as needed but does not initialize it.
	 * \param ID type ID of the requested data (as returned by util::typeID<T>())
	 * \param len amount of elements in the array
	 * \returns a Reference to a ValueArray pointing to the allocated memory. Or an empty Reference if the creation failed.
	 */
	static ValueArray createByID(unsigned short ID, size_t len );

	/**
	 * Copy this to a new TypedArray\<T\> using newly allocated memory.
	 * This will create a new ValueArray of type T and the length of this.
	 * The memory will be allocated and the data of this will be copy-converted to T using min/max as value range.
	 * If the conversion fails, an error will be send to CoreLog and the data of the newly created ValueArray will be undefined.
	 * \returns a the newly created ValueArray
	 */
	template<typename T> [[nodiscard]] ValueArray copyAs(const scaling_pair& scaling = scaling_pair() )const {
		checkType<T>();
		return copyByID( util::typeID<T>(), scaling );;
	}

	/**
	 * Get this as a ValueArray of a specific type.
	 * This does an automatic conversion into a new ValueArray if one of following is true:
	 * - the target type is not the current type
	 * - scaling.first (the scaling factor) is not 1
	 * - scaling.first (the scaling offset) is not 0
	 *
	 * Otherwise a cheap copy is done.
	 * \param ID the ID of the requested type (use ValueArray::staticID())
	 * \param scaling the scaling to be used (determined automatically if not given)
	 * \returns a reference of either a cheap copy or a newly created ValueArray
	 * \returns an empty reference if the conversion failed
	 */
	[[nodiscard]] ValueArray  convertByID(unsigned short ID, scaling_pair scaling = scaling_pair() )const;


	/**
	 * Get this as a ValueArray of a specific type.
	 * This does an automatic conversion into a new ValueArray if one of following is true:
	 * - the target type is not the current type
	 * - scaling.first (the scaling factor) is not 1
	 * - scaling.first (the scaling offset) is not 0
	 *
	 * Otherwise a cheap copy is done.
	 * \param scaling the scaling to be used (determined automatically if not given)
	 * \returns either a cheap copy or a newly created ValueArray
	 */
	template<typename T> [[nodiscard]] ValueArray as(const scaling_pair &scaling = scaling_pair() )const {
		checkType<T>();
		return convertByID( util::typeID<T>(), scaling );
	}

	/**
	 * Create a new ValueArray, of the same type, but different size in memory.
	 * (The actual data are _not_ copied)
	 * \param length length of the new memory block in elements of the given TYPE
	 */
	[[nodiscard]] ValueArray cloneToNew(size_t length )const;

	/// \returns the byte-size of the type of the data this ValueArray points to.
	[[nodiscard]] size_t bytesPerElem()const;

	/**
	 * Copy a range of elements to another ValueArray of the same type.
	 * \param start first element in this to be copied
	 * \param end last element in this to be copied
	 * \param dst target for the copy
	 * \param dst_start starting element in dst to be overwritten
	 */
	void copyRange(size_t start, size_t end, ValueArray &dst, size_t dst_start )const;

	/// \returns the number of references using the same memory as this.
	[[nodiscard]] size_t useCount()const;

	/**
	 * Get minimum/maximum of a ValueArray.
	 * This computes the minimum and maximum value of the stored data and stores them in ValueReference-Objects.
	 * This actually returns the bounding box of the values in the value space of the type. This means:
	 * - min/max numbers for numbers from a 1-D value space (aka real numbers)
	 * - complex(lowest real value,lowest imaginary value) / complex(biggest real value,biggest imaginary value) for complex numbers
	 * - color(lowest red value,lowest green value, lowest blue value)/color(biggest red value,biggest green value, biggest blue value) for color
	 * The computed min/max are of the same type as the stored data, but can be compared to other ValueReference without knowing this type via the lt/gt function of Value.
	 * The following code checks if the value range of ValueArray-object data1 is a real subset of data2:
	 * \code
	 * std::pair<util::Value,util::Value> minmax1=data1.getMinMax(), minmax2=data2.getMinMax();
	 * if(minmax1.first->gt(minmax2.second) && minmax1.second->lt(minmax2.second)
	 *  std::cout << minmax1 << " is a subset of " minmax2 << std::endl;
	 * \endcode
	 * \returns a pair of ValueReferences referring to the found minimum/maximum of the data
	 */
	[[nodiscard]] std::pair<util::Value, util::Value> getMinMax()const;

	/**
	 * Compare the data of two ValueArray.
	 * Counts how many elements in this and the given ValueArray are different within the given range.
	 * If the type of this is not equal to the type of the given ValueArray the whole length is assumed to be different.
	 * If the given range does not fit into this or the given ValueArray an error is send to the runtime log and the function will probably crash.
	 * \param start the first element in this, which should be compared to the first element in the given compare destination
	 * \param end the first element in this, which should _not_ be compared anymore to the given compare ValueArray
	 * \param dst the given destination ValueArray this should be compared to
	 * \param dst_start the first element in the given destination ValueArray, which should be compared to the first element in this
	 * \returns the amount of elements which actually differ in both ValueArray or the whole length of the range when the types are not equal.
	 */
	size_t compare(size_t start, size_t end, const ValueArray &dst, size_t dst_start )const;

	[[nodiscard]] bool isValid()const;

	/// return a shared pointer to void with optional offset in bytes
	virtual std::shared_ptr<void> getRawAddress( size_t offset = 0 );
	[[nodiscard]] virtual std::shared_ptr<const void> getRawAddress( size_t offset = 0 )const;

	/**
	* Dynamically cast the ValueArray up to its actual TypedArray\<T\>. Constant version.
	* Will send an error if T is not the actual type and _ENABLE_CORE_LOG is true.
	* \returns a constant reference of the ValueArray.
	*/
	template<typename T> const std::shared_ptr<T>& castTo() const {
		checkType<T>();
		LOG_IF(!is<T>(),Debug,error) << "Trying to cast " << typeName() << " as " << util::typeName<T>() << " this will crash";
		return std::get<std::shared_ptr<T>>(*this);
	}

	/**
	 * Dynamically cast the ValueArray to the underlying std::shared_ptr\<T\>. Referenced version.
	 * Will send an error if T is not the actual type and _ENABLE_CORE_LOG is true.
	 * \returns a reference of the std::shared_ptr\<T\>.
	 */
	template<typename T> std::shared_ptr<T>& castTo() {
		LOG_IF(!is<T>(),Debug,error) << "Trying to cast " << typeName() << " as " << util::typeName<T>() << " this will crash";
		checkType<T>();
		return std::get<std::shared_ptr<T>>(*this);
	}

	iterator begin();
	[[nodiscard]] const_iterator begin()const;
	iterator end();
	[[nodiscard]] const_iterator end()const;

	template<typename T> const T* beginTyped()const{return castTo<T>().get();}
	template<typename T> const T* endTyped()const{return castTo<T>().get()+m_length;}
	template<typename T> T* beginTyped(){return castTo<T>().get();}
	template<typename T> T* endTyped(){return castTo<T>().get()+m_length;}

    [[nodiscard]] const_iterator::difference_type getDistanceTo(const const_iterator &it)const{
        //actually generic iterators know their starting point better than we do
        return it.getMyDistance();
    }

    template<typename T> T& at(size_t pos){return *(beginTyped<T>()+pos);}
	template<typename T> const T& at(size_t pos)const{return *(beginTyped<T>()+pos);}

	/// @copydoc util::Value::toString
	[[nodiscard]] std::string toString( bool labeled = false )const;

	[[nodiscard]] bool isFloat() const;
	[[nodiscard]] bool isInteger() const;

	void endianSwap();

	friend std::ostream &operator<<(std::ostream &os, const ValueArray &array);
/// delete-functor which does nothing (in case someone else manages the data).
	struct NonDeleter {
		template<typename T> void operator()( T *p )const {
			//we have to cast the pointer to void* here, because in case of uint8_t it will try to print the "string"
			LOG( Debug, verbose_info ) << "Not freeing pointer " << ( void * )p << " (" << util::typeName<T>() << ") as automatic deletion was disabled for it";
		};
	};
};

}

