#ifndef VALUE_HPP
#define VALUE_HPP

#include <iostream>
#include <typeindex>
#include <array>

#include "valuetypes.hpp"
#include "value_converter.hpp"

namespace isis::util{

namespace _internal{
template<typename charT, typename traits> struct print_visitor{
    std::basic_ostream<charT, traits> &out;
    template<typename T> void operator()(const T &v){
        out << v;
    }
};
}

class ValueNew:public ValueTypes{
	static const _internal::ValueConverterMap &converters();
public:
	typedef _internal::ValueConverterMap::mapped_type::mapped_type Converter;
    
    template<int I> using TypeByIndex = typename std::variant_alternative<I, ValueTypes>::type;

	template<typename T> ValueNew(T &&v):ValueTypes(v){}
    ValueNew(const ValueTypes &v);
    ValueNew(ValueTypes &&v);
	template<typename T> ValueNew& operator=(const T& v){ValueTypes::operator=(v);return *this;}
	template<typename T> ValueNew& operator=(T&& v){ValueTypes::operator=(v);return *this;}
	ValueNew();
    std::string typeName()const;
    template<typename T> static std::string staticName(){
        return ValueNew(T()).typeName();
    }
    template<typename T> static constexpr std::size_t staticIndex(){
        return ValueTypes(T()).index();
    }

	template<typename charT, typename traits>
    std::ostream &print(bool with_typename=true,std::basic_ostream<charT, traits> &out=std::cout)const{
		std::visit(_internal::print_visitor<charT,traits>{out},static_cast<const ValueTypes&>(*this));
		if(with_typename)
			out << "(" << typeName() << ")";
		return out;
	}

	/// \return true if the stored type is T
	template<typename T> bool is()const{
		return std::holds_alternative<T>(*this);
	}

	const Converter &getConverterTo( unsigned short ID )const;

    std::string toString(bool with_typename=true)const;

	/// creates a copy of the stored value using a type referenced by its ID
	ValueNew copyByID( unsigned short ID ) const;

	/// creates a default constructed value using a type referenced by its ID
	static ValueNew createByID( unsigned short ID );

	/**
	 * Check if the stored value would also fit into another type referenced by its ID
	 * \returns true if the stored value would fit into the target type, false otherwise
	 */
	bool fitsInto( unsigned short ID ) const;

	/**
	 * Convert the content of one Value to another.
	 * This will use the automatic conversion system to transform the value one Value-Object into another.
	 * The types of both objects can be unknown.
	 * \param from the Value-object containing the value which should be converted
	 * \param to the Value-object which will contain the converted value if conversion was successfull
	 * \returns false if the conversion failed for any reason, true otherwise
	 */
	static bool convert( const ValueNew &from, ValueNew &to );

	/**
	* Interpret the value as value of any (other) type.
	* This is a runtime-based cast via automatic conversion.
	* \code
	* ValueBase *mephisto=new Value<std::string>("666");
	* int devil=mephisto->as<int>();
	* \endcode
	* If you know the type of source and destination at compile time you should use Value\<DEST_TYPE\>((SOURCE_TYPE)src).
	* \code
	* Value<std::string> mephisto("666");
	* Value<int> devil((std::string)mephisto);
	* \endcode
	* \return this value converted to the requested type if conversion was successfull.
	*/
	template<class T> T as()const {
		if( is<T>() )
			return std::get<T>(*this);

		try{
			ValueNew ret = copyByID( ValueNew::staticIndex<T>() );
			return std::get<T>(ret);
		} catch(...) {//@todo specify exception
			LOG( Debug, error )
					<< "Interpretation of " << toString( true ) << " as " << ValueNew::staticName<T>()
					<< " failed. Returning " << ValueNew(T()).toString() << ".";
			return T();
		} 
	}
	
	bool isFloat()const;
	bool isInteger()const;

	/**
	 * Check if the this value is greater to another value converted to this values type.
	 * The function tries to convert ref to the type of this and compares the result.
	 * If there is no conversion an error is send to the debug logging, and false is returned.
	 * \retval value_of_this>converted_value_of_ref if the conversion was successfull
	 * \retval true if the conversion failed because the value of ref was to low for TYPE (negative overflow)
	 * \retval false if the conversion failed because the value of ref was to high for TYPE (positive overflow)
	 * \retval false if there is no know conversion from ref to TYPE
	 */
	bool gt( const ValueNew &ref )const;

	/**
	 * Check if the this value is less than another value converted to this values type.
	 * The funkcion tries to convert ref to the type of this and compare the result.
	 * If there is no conversion an error is send to the debug logging, and false is returned.
	 * \retval value_of_this<converted_value_of_ref if the conversion was successfull
	 * \retval false if the conversion failed because the value of ref was to low for TYPE (negative overflow)
	 * \retval true if the conversion failed because the value of ref was to high for TYPE (positive overflow)
	 * \retval false if there is no know conversion from ref to TYPE
	 */
	bool lt( const ValueNew &ref )const;

	/**
	 * Check if the this value is equal to another value converted to this values type.
	 * The funktion tries to convert ref to the type of this and compare the result.
	 * If there is no conversion an error is send to the debug logging, and false is returned.
	 * \retval value_of_this==converted_value_of_ref if the conversion was successfull
	 * \retval false if the conversion failed because the value of ref was to low for TYPE (negative overflow)
	 * \retval false if the conversion failed because the value of ref was to high for TYPE (positive overflow)
	 * \retval false if there is no known conversion from ref to TYPE
	 */
	bool eq( const ValueNew &ref )const;

	ValueNew plus( const ValueNew &ref )const;
	ValueNew minus( const ValueNew &ref )const;
	ValueNew multiply( const ValueNew &ref )const;
	ValueNew divide( const ValueNew &ref )const;

	ValueNew& add( const ValueNew &ref );
	ValueNew& substract( const ValueNew &ref );
	ValueNew& multiply_me( const ValueNew &ref );
	ValueNew& divide_me( const ValueNew &ref );
	
	/**
	 * Set value to a new value but keep its type.
	 * This will convert the applied value to the current type.
	 * If this conversion fails nothing is done and false is returned.
	 * \returns true if conversion was successfull (and the value was changed), false otherwise
	 */
	bool apply(const ValueNew &other);
};

}

// streaming output
namespace std
{
template<typename charT, typename traits>
basic_ostream<charT, traits>& operator<<( basic_ostream<charT, traits> &out, const isis::util::ValueNew &s )
{
	return s.print(true,out);
}
}


#endif // VALUE_HPP
