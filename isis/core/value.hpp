#pragma once

#include <iostream>
#include <typeindex>
#include <array>

#include "types_value.hpp"
#include "value_converter.hpp"
#include <functional>

namespace isis::data::_internal{
    class ConstValueAdapter;
	class WritingValueAdapter;
}

namespace isis::util{

class ValueNew:public ValueTypes{
	static const _internal::ValueConverterMap &converters();
	template<typename OP, typename RET> RET operatorWrapper(const OP& op,const ValueNew &rhs,const RET &default_ret)const{
		try{
			return op(*this,rhs);
		} catch(const std::domain_error &e){ // return default value on failure
			LOG(Runtime,error) << "Operation " << MSubject( typeid(OP).name() ) << " on " << MSubject( typeName() ) << " and " << MSubject( rhs.typeName() ) << " failed with \"" << e.what()
			<< "\", will return " << MSubject( ValueNew(default_ret).toString(true) );
		return default_ret;
		}
	}
	template<typename OP> ValueNew& operatorWrapper_me(const OP& op,const ValueNew &rhs){
		try{
			op(*this,rhs);
		} catch(const std::domain_error &e){
			LOG(Runtime,error) << "Operation " << MSubject( typeid(OP).name() ) << " on " << MSubject( typeName() ) << " and " << MSubject( rhs.typeName() ) << " failed with \"" << e.what()
			<< "\", wont change value (" << MSubject( this->toString(true) ) << ")";
		}
		return *this;
	}

public:
	typedef _internal::ValueConverterMap::mapped_type::mapped_type Converter;

	template<int I> using TypeByIndex = typename std::variant_alternative<I, ValueTypes>::type;

	template <typename T, std::enable_if_t<!std::is_base_of_v<isis::data::_internal::ConstValueAdapter,T>, int> = 0>//@clean up this mess
	constexpr ValueNew(T &&v):ValueTypes(v){}
	ValueNew(const isis::data::_internal::ConstValueAdapter &v);
	ValueNew(const ValueTypes &v);
	ValueNew(ValueTypes &&v);
	template<typename T> ValueNew& operator=(const T& v){ValueTypes::operator=(v);return *this;}
	template<typename T> ValueNew& operator=(T&& v){ValueTypes::operator=(v);return *this;}
	ValueNew()=default;

	std::string typeName()const;
	size_t typeID()const;

	/// \return true if the stored type is T
	template<typename T> bool is()const{
		return std::holds_alternative<T>(*this);
	}

	const Converter &getConverterTo( unsigned short ID )const;

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
			ValueNew ret = copyByID( util::typeID<T>() );
			return std::get<T>(ret);
		} catch(...) {//@todo specify exception
			LOG( Debug, error )
			        << "Interpretation of " << toString( true ) << " as " << isis::util::typeName<T>()
			        << " failed. Returning " << ValueNew(T()).toString() << ".";
			return T();
		}
	}

	bool isFloat()const;
	bool isInteger()const;
	bool isValid()const;

	std::string toString(bool with_typename=true)const;

	template<typename charT, typename traits>
	std::ostream &print(bool with_typename=true,std::basic_ostream<charT, traits> &out=std::cout)const{
		std::visit([&](auto val){out << val;},static_cast<const ValueTypes&>(*this));
		if(with_typename)
			out << "(" << typeName() << ")";
		return out;
	}

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
	
	ValueNew operator+( const ValueNew &ref )const{return plus(ref);};
	ValueNew operator-( const ValueNew &ref )const{return minus(ref);};
	ValueNew operator*( const ValueNew &ref )const{return multiply(ref);};
	ValueNew operator/( const ValueNew &ref )const{return divide(ref);};

	ValueNew& operator+=( const ValueNew &ref ){return add(ref);};
	ValueNew& operator-=( const ValueNew &ref ){return substract(ref);};
	ValueNew& operator*=( const ValueNew &ref ){return multiply_me(ref);};
	ValueNew& operator/=( const ValueNew &ref ){return divide_me(ref);};
	
	bool operator<( const ValueNew &ref )const{return lt(ref);};
	bool operator>( const ValueNew &ref )const{return gt(ref);};
	bool operator==( const ValueNew &ref )const{return eq(ref);};

	/**
	 * Set value to a new value but keep its type.
	 * This will convert the applied value to the current type.
	 * If this conversion fails nothing is done and false is returned.
	 * \returns true if conversion was successful (and the value was changed), false otherwise
	 */
	bool apply(const ValueNew &other);
};


API_EXCLUDE_BEGIN;
/// @cond _internal
namespace _internal{
/**
 * Generic value operation class.
 * This generic class does nothing, and the ()-operator will allways fail with an error send to the debug-logging.
 * It has to be (partly) specialized for the regarding type.
 */
template<typename OPERATOR,bool modifying,bool enable> struct type_op
{
	typedef typename OPERATOR::result_type result_type;
	typedef std::integral_constant<bool,enable> enabled;
	typedef typename std::conditional<modifying,util::ValueNew,const util::ValueNew>::type lhs; //<typename OPERATOR::first_argument_type>

	result_type operator()( lhs &first, const ValueNew &second )const {
		LOG( Debug, error )
		    << "operator " << typeid(OPERATOR).name() << " is not supportet for "
		    << first.typeName()  << " and " << second.typeName();
		throw std::domain_error("operation not available");
	}
};

// compare operators (overflows are no error here)
template<typename OPERATOR,bool enable> struct type_comp_base : type_op<OPERATOR,false,enable>{
	typename OPERATOR::result_type posOverflow()const {return false;}
	typename OPERATOR::result_type negOverflow()const {return false;}
};
template<typename T> struct type_eq   : type_comp_base<std::equal_to<T>,true>{};
template<typename T> struct type_less : type_comp_base<std::less<T>,    has_op<T>::lt>
{
	//getting a positive overflow when trying to convert second into T, obviously means first is less
	typename std::less<T>::result_type posOverflow()const {return true;}
};
template<typename T> struct type_greater : type_comp_base<std::greater<T>,has_op<T>::gt>
{
	//getting an negative overflow when trying to convert second into T, obviously means first is greater
	typename std::greater<T>::result_type negOverflow()const {return true;}
};

// on-self operations .. we return void because the result won't be used anyway
template<typename OP> struct op_base : std::binary_function <typename OP::first_argument_type,typename OP::second_argument_type,void>{};

template<typename T> struct plus_op :  op_base<std::plus<T> >      {void operator() (typename std::plus<T>::first_argument_type& x,       typename std::plus<T>::second_argument_type const& y)       const {x+=y;}};
template<typename T> struct minus_op : op_base<std::minus<T> >     {void operator() (typename std::minus<T>::first_argument_type& x,      typename std::minus<T>::second_argument_type const& y)      const {x-=y;}};
template<typename T> struct mult_op :  op_base<std::multiplies<T> >{void operator() (typename std::multiplies<T>::first_argument_type& x, typename std::multiplies<T>::second_argument_type const& y) const {x*=y;}};
template<typename T> struct div_op :   op_base<std::divides<T> >   {void operator() (typename std::divides<T>::first_argument_type& x,    typename std::divides<T>::second_argument_type const& y)    const {x/=y;}};

template<typename T> struct type_plus :  type_op<plus_op<T>,true, has_op<T>::plus>{};
template<typename T> struct type_minus : type_op<minus_op<T>,true,has_op<T>::minus>{};
template<typename T> struct type_mult :  type_op<mult_op<T>,true, has_op<T>::mult>{};
template<typename T> struct type_div :   type_op<div_op<T>,true,  has_op<T>::div>{};


/**
 * Half-generic value operation class.
 * This generic class does math operations on Values by converting the second Value-object to the type of the first Value-object. Then:
 * - if the conversion was successfull (the second value can be represented in the type of the first) the "inRange"-operation is used
 * - if the conversion failed with an positive or negative overflow (the second value is to high/low to fit into the type of the first) a info sent to the debug-logging and the posOverflow/negOverflow operation is used
 * - if there is no known conversion from second to first an error is sent to the debug-logging and std::domain_error is thrown.
 * \note The functions (posOverflow,negOverflow) here are only stubs and will allways throw std::domain_error.
 * \note inRange will return OPERATOR()(first,second)
 * These class can be further specialized for the regarding operation.
 */
template<typename OPERATOR,bool modifying> struct type_op<OPERATOR,modifying,true>
{
	virtual ~type_op(){}
	typedef typename std::conditional<modifying,util::ValueNew,const util::ValueNew>::type lhs; //<typename OPERATOR::first_argument_type>
	typedef typename util::ValueNew rhs; //<typename OPERATOR::second_argument_type>
	typedef typename OPERATOR::result_type result_type;
	typedef std::integral_constant<bool,true> enabled;

	virtual result_type posOverflow()const {throw std::domain_error("positive overflow");}
	virtual result_type negOverflow()const {throw std::domain_error("negative overflow");}
	virtual result_type inRange( lhs &first, const util::ValueNew &second )const {
		return OPERATOR()(std::get<typename OPERATOR::first_argument_type>(first),second.as<typename OPERATOR::second_argument_type>());
	}
	result_type operator()(lhs &first, const ValueNew &second )const {
		// ask second for a converter from itself to Value<T>
		const ValueNew::Converter conv = second.getConverterTo( util::typeID<typename OPERATOR::second_argument_type>() );

		if ( conv ) {
			//try to convert second into T and handle results
			rhs buff;

			switch ( conv->convert( second, buff ) ) {
			    case boost::numeric::cPosOverflow:return posOverflow();
			    case boost::numeric::cNegOverflow:return negOverflow();
			    case boost::numeric::cInRange:
				    LOG_IF(second.isFloat() && second.as<float>()!=buff.as<float>(), Debug,warning)
					<< "Using " << second.toString( true ) << " as " << buff.toString( true ) << " for operation on " << first.toString( true )
					<< " you might loose precision";
				    return inRange( first, buff );
			}
		}
		throw std::domain_error(util::typeName<typename OPERATOR::second_argument_type>()+" not convertible to "+second.typeName());
	}
};
}
/// @endcond _internal
API_EXCLUDE_END;

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
