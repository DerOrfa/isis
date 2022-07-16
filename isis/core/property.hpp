//
// C++ Interface: property
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#pragma once

#include "value.hpp"
#include "log.hpp"

namespace isis::util
{

/**
 * A very generic class to store values of properties.
 * PropertyValue may store a value of any type (defined in types.cpp) otherwise it's empty.
 * Non-empty ValueValues are equal-comparable.
 * But empty PropertyValues are neither equal nor unequal to anything (not even to empty ValueValues).
 * @author Enrico Reimer
 */
class PropertyValue
{
private:
	bool m_needed=false;
	std::list<Value> container;
	template<typename OP> [[nodiscard]] PropertyValue operator_impl(const PropertyValue &rhs)const;
	template<typename OP> [[nodiscard]] PropertyValue operator_impl(const Value &rhs)const;
public:
	typedef decltype(container)::iterator iterator;
	typedef decltype(container)::const_iterator const_iterator;
	typedef decltype(container)::reference reference;
	typedef decltype(container)::const_reference const_reference;
	typedef decltype(container)::value_type value_type;
	/// Create a property and store the given single value object.
	PropertyValue(const Value &ref, bool _needed = false ): m_needed(_needed ), container(1,ref) {}
	/// Create a property and store the given single value object.
	PropertyValue(Value &&ref, bool _needed = false ): m_needed(_needed ){container.emplace_back(ref);}

	////////////////////////////////////////////////////////////////////////////
	// List operations
	////////////////////////////////////////////////////////////////////////////
	PropertyValue::iterator push_back(Value&& ref);

	iterator insert(iterator at,Value&& ref);
	template<typename InputIterator> void insert( iterator position, InputIterator first, InputIterator last ){container.insert(position,first,last);}

	iterator erase( size_t at );
	iterator erase( iterator first, iterator last );

	void resize(size_t size, const Value& insert={});

	/**
	 * Increase the size of the property by replacing every entry by factor results of op(original_value).
	 * op is guaranteed to be run sequential, so internal increments can be used.
	 * @param factor amount of entries each entry should be replaced with
	 * @param op functor to compute the new entries from the old one
	 * @return new length of the property
	 */
	size_t explode(size_t factor, const std::function<Value(const Value &)>& op);

// 	void reserve(size_t size);
// 	void resize( size_t size, const Value& clone );
	[[nodiscard]] Value&        operator[](size_t n );
	[[nodiscard]] const Value&  operator[](size_t n ) const;
	[[nodiscard]] Value&        at(size_t n );
	[[nodiscard]] const Value&  at(size_t n ) const;
	[[nodiscard]] Value&        front();
	[[nodiscard]] const Value&  front()const;

	const Value &operator()()const;
	Value &operator()();


	[[nodiscard]] iterator begin();
	[[nodiscard]] const_iterator begin()const;

	[[nodiscard]] iterator end();
	[[nodiscard]] const_iterator end()const;

	/**
	 * Splice up the property value.
	 * Distribute entries into new PropertyValues of given equal length.
	 * \note the last PropertyValue may have less entries (aka remainder)
	 * \note this is a transfer function, so *this will be empty afterwards.
	 * \param len the requested size of the "splinters"
	 * \returns a vector of (mostly) equally sized PropertyValues.
	 */
	[[nodiscard]] std::vector<PropertyValue> splice(size_t len);

	/// Amount of values in this PropertyValue
	[[nodiscard]] size_t size()const;

	/**
	 * Transfer properties from one PropertyValue into another.
	 * The transferred data will be inserted at idx.
	 * The properties in the target will not be removed. Thus the PropertyValue will grow.
	 * The source will be empty afterwards.
	 */
	void transfer(iterator idx,PropertyValue &ref);
	/**
	 * Transfer properties from another PropertyValue to this.
	 * The source will be empty afterwards.
	 * \param src the PropertyValue to transfer data from
	 * \param overwrite if transferred data shall replace existing data
	 */
	void transfer(PropertyValue &src, bool overwrite=false);

	/// Swap properties from one PropertyValue with another.
	void swap(PropertyValue &src);

	/// Transform all contained properties into type T
	bool transform( uint16_t dstID );
	template<KnownValueType T> bool transform(){return transform(typeID<T>());}

	/**
	 * Empty constructor.
	 * Creates an empty property value. So PropertyValue().isEmpty() will always be true.
	 */
	PropertyValue()=default;

	/**
	 * Copy operator.
	 * Copies the content of another Property.
	 * \param other the source to copy from
	 * \note the needed state will be copied
	 */
	PropertyValue(const PropertyValue&)=default;
	PropertyValue(PropertyValue&&)=default;
	/**
	 * Copy operator.
	 * Copies the content of another Property.
	 * \param other the source to copy from
	 * \note the needed state wont change, regardless of what it is in other
	 */
	PropertyValue &operator=(const PropertyValue &other)=default;
	PropertyValue &operator=(PropertyValue &&other)=default;

	/// mark as (not) needed
	void setNeeded(bool needed=true);
	/// returns true if PropertyValue is marked as needed, false otherwise
	[[nodiscard]] bool isNeeded ()const;

	/**
	 * Equality to another PropertyValue.
	 * Properties are ONLY equal if:
	 * - both properties are not empty
	 * - both properties contain the same amount of values
	 * - \link Value::operator== \endlink is true for all stored values
	 * - (which also means equal types)
	 * \note Empty properties are neither equal nor unequal
	 * \returns true if both contain the same values of the same type, false otherwise.
	 */
	bool operator ==( const PropertyValue &second )const;
	/**
	 * Unequality to another PropertyValue.
	 * Properties are unequal if:
	 * - they have different amounts of values stored
	 * - or operator!= is true on any stored value
	 * \note Empty properties are neither equal nor unequal
	 * \returns false if operator!= is not true for all stored value and amount of values is equal
	 * \returns true otherwise
	 */
	bool operator !=( const PropertyValue &second )const;
	/**
	 * Equality to another Value-Object
	 * \returns Value::operator== if the property has exactly one value, false otherwise.
	 */
	bool operator ==( const Value &second )const;
	/**
	 * Unequality to another Value-Object
	 * \returns Value::operator!= if the property has exactly one value, false otherwise.
	 */
	bool operator !=( const Value &second )const;

	/**
	 * (re)set property to one specific value of a specific type
	 * \note The needed flag won't be affected by that.
	 * \note To prevent accidental use this can only be used explicitly. \code util::PropertyValue propA; propA=5; \endcode is valid. But \code util::PropertyValue propA=5; \endcode is not,
	 */
	PropertyValue& operator=(Value &&ref);

	/**
	 * creates a copy of the stored values using a type referenced by its ID
	 * \returns a new PropertyValue containing all values converted to the requested type
	 */
	[[nodiscard]] PropertyValue copyByID( unsigned short ID ) const;

	/// \returns the value(s) represented as text.
	[[nodiscard]] std::string toString( bool labeled=false)const;

	/// \returns true if, and only if no value is stored
	[[nodiscard]] bool isEmpty()const;


	////////////////////////////////////////////////////////////////////////////
	// Value interface
	////////////////////////////////////////////////////////////////////////////

	/**
	 * Hook for \link Value::as \endlink called on the first value
	 * \note Uses only the first value. Other Values are ignored (use the []-operator to access them).
	 * \note An exception is thrown if the PropertyValue is empty.
	 */
	template<KnownValueType T> T as()const {return front().as<T>();}

	template<KnownValueType T> T& castAs() {return std::get<T>(front());}

	/**
	 * \copybrief Value::is
	 * hook for \link Value::is \endlink
	 * \note Applies only on the first value. Other Values are ignored (use the []-operator to access them).
	 * \note An exception is thrown if the PropertyValue is empty.
	 */
	template<KnownValueType T> [[nodiscard]] bool is()const {return container.front().is<T>();}

	/**
	 * \copybrief Value::getTypeName
	 * hook for \link Value::getTypeName \endlink
	 * \note Applies only on the first value. Other Values are ignored (use the []-operator to access them).
	 * \note An exception is thrown if the PropertyValue is empty.
	 */
	[[nodiscard]] std::string getTypeName()const;

	/**
	 * \copybrief Value::getTypeID
	 * hook for \link Value::getTypeID \endlink
	 * \note Applies only on the first value. Other Values are ignored (use the []-operator to access them).
	 * \note An exception is thrown if the PropertyValue is empty.
	 */
	[[nodiscard]] unsigned short getTypeID()const;

	/**
	 * Cast to a known type.
	 * \returns result of std::get\<T\> on first on the first value.
	 * \note Other Values are ignored (use the []-operator to access them).
	 * \note An exception is thrown if the PropertyValue is empty.
	 */
	template<KnownValueType T> const T &castTo() const{return std::get<T>(front());}

	/**
	 * \returns true if \link Value::fitsInto \endlink is true for all values
	 * \note Operation is done on all values. For comparing single values access them via the []-operator.
	 */
	[[nodiscard]] bool fitsInto( unsigned short ID ) const;

	/**
	 * \returns true if \link Value::gt \endlink is true for all values
	 * \note Operation is done on all values. For working on single values access them via the []-operator.
	 * \note An exception is thrown if this has less values than the target (the opposite case is ignored).
	 */
	[[nodiscard]] bool gt( const PropertyValue &ref )const;
	/**
	 * \returns true if \link Value::lt \endlink is true for all values
	 * \copydetails PropertyValue::gt
	 */
	[[nodiscard]] bool lt( const PropertyValue &ref )const;
	/**
	 * \returns true if \link Value::eq \endlink is true for all values
	 * \copydetails PropertyValue::gt
	 */
	[[nodiscard]] bool eq( const PropertyValue &ref )const;

	/**
	 * \returns a PropertyValue with the results of \link Value::operator+ \endlink done on all value pairs from this and the target
	 * \copydetails PropertyValue::gt
	 */
	[[nodiscard]] PropertyValue plus( const PropertyValue &ref )const;
	[[nodiscard]] PropertyValue plus( const Value &ref )const;
	/**
	 * \returns a PropertyValue with the results of \link Value::operator- \endlink done on all value pairs from this and the target
	 * \copydetails PropertyValue::gt
	 */
	[[nodiscard]] PropertyValue minus( const PropertyValue &ref )const;
	[[nodiscard]] PropertyValue minus( const Value &ref )const;
	/**
	 * \returns a PropertyValue with the results of \link Value::operator* \endlink done on all value pairs from this and the target
	 * \copydetails PropertyValue::gt
	 */
	[[nodiscard]] PropertyValue multiply( const PropertyValue &ref )const;
	[[nodiscard]] PropertyValue multiply( const Value &ref )const;
	/**
	 * \returns a PropertyValue with the results of \link Value::operator/ \endlink done on all value pairs from this and the target
	 * \copydetails PropertyValue::gt
	 */
	[[nodiscard]] PropertyValue divide( const PropertyValue &ref )const;
	[[nodiscard]] PropertyValue divide( const Value &ref )const;

	////////////////////////////////////////////////////////////////////////////
	// operators on "normal" values
	////////////////////////////////////////////////////////////////////////////

	PropertyValue& operator +=( const Value &second );
	PropertyValue& operator -=( const Value &second );
	PropertyValue& operator *=( const Value &second );
	PropertyValue& operator /=( const Value &second );

	// @todo document different behaviour from named operations (especially that it might change type
	PropertyValue operator+( const Value &second )const;
	PropertyValue operator-( const Value &second )const;
	PropertyValue operator*( const Value &second )const;
	PropertyValue operator/( const Value &second )const;

	/// streaming output for PropertyValue
	friend std::ostream& operator<<( std::ostream & out, const isis::util::PropertyValue &s );

	bool operator<(const isis::util::PropertyValue& y)const;
};

}
