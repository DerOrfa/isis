#pragma once

#include <iostream>
#include <typeindex>
#include <array>

#include "types_value.hpp"
#include <functional>
#include <complex>
#include <compare>

namespace isis::util
{
template<typename T> concept KnownValueType = _internal::variant_idx<T>(static_cast<ValueTypes*>(nullptr))<std::variant_size_v<ValueTypes>;

namespace _internal
{
class ValueConverterMap;
class ValueConverterBase;
template<typename T> std::string_view typename_with_fallback()
{
	if constexpr(KnownValueType<T>)return typeName<T>();
	else return typeid(T).name();
}
}

class Value: public ValueTypes
{
	static const _internal::ValueConverterMap &converters();

	[[nodiscard]] std::partial_ordering converted_three_way_compare(const Value &v) const;
	[[nodiscard]] bool converted_equal_compare(const Value &v) const;
public:
	std::partial_ordering operator<=>(const Value &rhs) const;
	bool operator==(const Value& v)const;
	// it is actually better to have an explicit '==' operator as it's potentially faster and more often a valid operation

	typedef std::shared_ptr<const isis::util::_internal::ValueConverterBase> Converter;

	template<int I> using TypeByIndex = typename std::variant_alternative<I, ValueTypes>::type;
	static constexpr auto NumOfTypes =  std::variant_size_v<ValueTypes>;

	// default constructors
	Value()=default;
	Value(Value &&v)=default;
	Value(const Value &v)=default;

	Value(const std::string_view &v);

	// default assignment
	Value &operator=(const Value&)=default;
	Value &operator=(Value&&)=default;

	constexpr Value(KnownValueType auto &&v):ValueTypes(v){}
	constexpr Value(const KnownValueType auto &v):ValueTypes(v){}

	[[nodiscard]] std::string typeName()const;
	[[nodiscard]] size_t typeID()const;

	/// \return true if the stored type is T
	template<typename T> [[nodiscard]] bool is()const{
		return std::holds_alternative<T>(*this);
	}

	[[nodiscard]] const Converter &getConverterTo( unsigned short ID )const;

	/// creates a copy of the stored value using a type referenced by its ID
	[[nodiscard]] Value copyByID(size_t ID ) const;

	/// creates a default constructed value using a type referenced by its ID
	static Value createByID(unsigned short ID );

	/**
	 * Check if the stored value would also fit into another type referenced by its ID
	 * \returns true if the stored value would fit into the target type, false otherwise
	 */
	[[nodiscard]] bool fitsInto(size_t ID ) const;

	/**
	 * Convert the content of one Value to another.
	 * This will use the automatic conversion system to transform the value one Value-Object into another.
	 * The types of both objects can be unknown.
	 * \param from the Value-object containing the value which should be converted
	 * \param to the Value-object which will contain the converted value if conversion was successful
	 * \returns false if the conversion failed for any reason, true otherwise
	 */
	static bool convert(const Value &from, Value &to );

	/**
	* Interpret the value as value of any (other) type.
	* This is a runtime-based cast via automatic conversion.
	* \code
	* Value mephisto("666");
	* int devil=mephisto->as<int>();
	* \endcode
	* \return this value converted to the requested type if conversion was successful.
	*/
	template<class T> [[nodiscard]] T as()const {
		if( is<T>() )
			return std::get<T>(*this);

		try{
			Value ret = copyByID(util::typeID<T>() );
			return std::get<T>(ret);
		} catch(...) {//@todo specify exception
			LOG( Debug, error )
				<< "Interpretation of " << *this << " as " << isis::util::typeName<T>()
				<< " failed. Returning " << Value(T()).toString() << ".";
			return T();
		}
	}

	[[nodiscard]] bool isFloat()const;
	[[nodiscard]] bool isInteger()const;
	[[nodiscard]] bool isValid()const;

	[[nodiscard]] std::string toString(bool with_typename=false)const;

	template<typename charT, typename traits>
	std::ostream &print(bool with_typename=true,std::basic_ostream<charT, traits> &out=std::cout)const{
		out << as<std::string>();
		if(with_typename)
			out << "(" << typeName() << ")";
		return out;
	}

	/**
	 * Check if the this value is greater to another value converted to this values type.
	 * The function tries to convert ref to the type of this and compares the result.
	 * If there is no conversion an error is send to the debug logging, and false is returned.
	 * \retval value_of_this>converted_value_of_ref if the conversion was successful
	 * \retval true if the conversion failed because the value of ref was to low for TYPE (negative overflow)
	 * \retval false if the conversion failed because the value of ref was to high for TYPE (positive overflow)
	 * \retval false if there is no know conversion from ref to TYPE
	 */
	[[nodiscard]] bool gt( const Value &ref )const;

	/**
	 * Check if the this value is less than another value converted to this values type.
	 * The function tries to convert ref to the type of this and compare the result.
	 * If there is no conversion an error is send to the debug logging, and false is returned.
	 * \retval value_of_this<converted_value_of_ref if the conversion was successful
	 * \retval false if the conversion failed because the value of ref was to low for TYPE (negative overflow)
	 * \retval true if the conversion failed because the value of ref was to high for TYPE (positive overflow)
	 * \retval false if there is no know conversion from ref to TYPE
	 */
	[[nodiscard]] bool lt( const Value &ref )const;

	/**
	 * Check if the this value is equal to another value converted to this values type.
	 * The function tries to convert ref to the type of this and compare the result.
	 * If there is no conversion an error is send to the debug logging, and false is returned.
	 * \retval value_of_this==converted_value_of_ref if the conversion was successful
	 * \retval false if the conversion failed because the value of ref was to low for TYPE (negative overflow)
	 * \retval false if the conversion failed because the value of ref was to high for TYPE (positive overflow)
	 * \retval false if there is no known conversion from ref to TYPE
	 */
	[[nodiscard]] bool eq( const Value &ref )const;

	// Value arithmetics
	// runs arithmetic operations (+,-,*,/) on the internal value and the given rhs
	// result type is defined by c++ rules (not necessarily the original value type)
	// +-operation with a string always results in a string
	[[nodiscard]] Value operator+(const Value &ref )const;
	[[nodiscard]] Value operator-(const Value &ref )const;
	[[nodiscard]] Value operator*(const Value &ref )const;
	[[nodiscard]] Value operator/(const Value &ref )const;

	Value operator+=(const Value &ref );;
	Value operator-=(const Value &ref );;
	Value operator*=(const Value &ref );;
	Value operator/=(const Value &ref );;

	/**
	 * Set value to a new value but keep its type.
	 * This will convert the applied value to the current type.
	 * If this conversion fails nothing is done and false is returned.
	 * \returns true if conversion was successful (and the value was changed), false otherwise
	 */
	bool apply(const Value &other);

	//implement standard ostream
	friend std::ostream& operator<<( std::ostream &out, const Value &s );

};

}
