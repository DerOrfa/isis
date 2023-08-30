#include "value.hpp"
#include "value_converter.hpp"

#include <sstream>

namespace isis::util{
namespace _internal{

// three-way comparison that excludes Value to prevent recursion
template<typename T> concept non_value = (!std::is_same_v<T, Value>);
template<typename T1, typename T2> concept three_way_comparable_with = requires(T1 &&v1, T2 &&v2){{ v1 <=> v2 } -> 	std::convertible_to<std::partial_ordering>;};
template<typename T1, typename T2> concept three_way_comparable_with_non_value = non_value<T1> && non_value<T2> && three_way_comparable_with<T1, T2>;
template<typename T1, typename T2> concept equal_comparable_with = requires(T1 &&v1, T2 &&v2){{ v1 == v2 } -> 	std::convertible_to<bool>;};;
template<typename T1, typename T2> concept equal_comparable_with_non_value = non_value<T1> && non_value<T2> && equal_comparable_with<T1, T2>;
template<typename T> concept three_way_comparable_non_value = three_way_comparable_with_non_value<T, T>;
template<typename T> concept equal_comparable_non_value = equal_comparable_with_non_value<T, T>;
template<typename OP, typename T1, typename T2=T1> concept op_available = requires(const std::remove_cvref_t<T1> &v1, const std::remove_cvref_t<T2> &v2){OP{}(v1,v2);};
template<typename OP, typename T1, typename T2=T1> concept op_available_non_value = non_value<T1> && non_value<T2> && op_available<OP,T1,T2>;

std::partial_ordering static_three_way_compare(const Value &lhs, const three_way_comparable_non_value auto &rhs)
{
	typedef std::remove_cvref_t<decltype(rhs)> r_type;
	auto visitor = [&rhs](const auto &lhs) -> std::partial_ordering
	{
		typedef std::remove_cvref_t<decltype(lhs)> l_type;
		//@todo std::compare_partial_order_fallback once it's widely available
		if constexpr(three_way_comparable_with<l_type, r_type>)
			return lhs <=> rhs;
		else if constexpr(std::is_convertible_v<r_type,l_type> && three_way_comparable_with<l_type, l_type>)
			return lhs <=> l_type(rhs);
		else if constexpr(std::is_convertible_v<l_type,r_type>) // we wouldn't be in here if three_way_comparable_with<r_type> wasn't true
			return r_type(lhs) <=> rhs;
		else
			return std::partial_ordering::unordered;
	};
	auto result = std::visit(visitor, static_cast<const ValueTypes &>(lhs));
	LOG_IF(result==std::partial_ordering::unordered,Runtime, error)
		<< "Cannot compare " << lhs.toString(true) << " and "
		<< rhs << "(" << _internal::typename_with_fallback<r_type>() << ")";

	return result;
}
bool static_equal_compare(const Value &lhs, const equal_comparable_non_value auto &rhs)
{
	typedef std::remove_cvref_t<decltype(rhs)> r_type;
	auto visitor = [&](auto &&ptr) -> bool
	{
		typedef std::remove_cvref_t<decltype(ptr)> l_type;
		if constexpr(equal_comparable_with<l_type, r_type>)
			return ptr == rhs;
		else{
			LOG(Runtime, error)
				<< "Cannot equal compare " << lhs.toString(true) << " and "
				<< rhs << "(" << _internal::typename_with_fallback<r_type>() << ")";
				return false;
		}
	};
	return std::visit(visitor, static_cast<const ValueTypes &>(lhs));
}
template<class OP> std::partial_ordering converted_compare(OP &&op,const Value &lhs, const Value &rhs)
{
	const auto &conv = rhs.getConverterTo( lhs.typeID() );
	Value to = Value::createByID(lhs.typeID());

	if ( conv ) {
		switch ( conv->convert( rhs, to ) ){
			case cInRange:return op(lhs,to);
			case cNegOverflow: return std::partial_ordering::greater;//v is so small it can't be represented in my type
			case cPosOverflow: return std::partial_ordering::less;//v is so big it can't be represented in my type
		}
	} else {
		LOG( Runtime, info )
			<< "I can't compare " << MSubject( lhs.toString( true ) ) << " to " << MSubject( rhs.toString(true) )
			<< " as " << rhs.typeName() << " can't be converted into " << lhs.typeName();
	}
	return std::partial_ordering::unordered;
}
template<class OP> [[nodiscard]] Value inner_arithmetic_op(const Value &l_value,const auto &rhs)
{
	static const OP op;
	auto visitor = [&rhs](const auto &lhs) -> Value
	{
		typedef std::remove_cvref_t<decltype(lhs)> l_type;
		typedef std::remove_cvref_t<decltype(rhs)> r_type;

		if constexpr(op_available_non_value<OP,l_type,r_type>) {
			auto result = op(lhs, rhs);
			if constexpr(KnownValueType<decltype(result)>)
				return result;
			else
				LOG(Runtime,error)
					<< "The result of the operation " << typeid(op).name() << " on "
					<< util::typeName<l_type>() << " and " << typename_with_fallback<r_type>()
					<< "(" << typename_with_fallback<decltype(result)>() << ") cannot be represented as Value. It will be ignored.";
		} else
			LOG(Runtime,error)
				<< "Invalid operation " << typeid(op).name() << " on " << util::typeName<l_type>()
				<< " and " << typename_with_fallback<r_type>();
		return lhs;
	};
	return std::visit(visitor,static_cast<const ValueTypes&>(l_value));
}
template<class OP> [[nodiscard]] Value arithmetic_op(const Value &l_value,const Value& r_value)
{
	auto visitor=[l_value](const auto &r_value)->Value{
		typedef std::remove_cvref_t<decltype(r_value)> r_type;
		if constexpr(op_available_non_value<OP,r_type>)
			return inner_arithmetic_op<OP>(l_value,r_value);
		else
			LOG(Runtime, error) << util::typeName<r_type>() << " cannot be used for arithmetics (the operation was: " << typeid(OP).name() <<  ")";
		return l_value;
	};
	return std::visit(visitor,static_cast<const ValueTypes&>(r_value));
}

}

std::string Value::toString(bool with_typename)const{
	std::stringstream o;
	print(with_typename,o);
	return o.str();
}

std::string Value::typeName() const {
	return std::visit(_internal::name_visitor(),static_cast<const ValueTypes&>(*this));
}
size_t Value::typeID()const{return index();}

const _internal::ValueConverterMap &Value::converters(){
	static _internal::ValueConverterMap ret;
	return ret;
}

const Value::Converter &Value::getConverterTo(unsigned short ID) const {
	const auto f1 = converters().find( typeID() );
	assert( f1 != converters().end() );
	const auto f2 = f1->second.find( ID );
	assert( f2 != f1->second.end() );
	return f2->second;
}

Value Value::createByID(unsigned short ID) {
	const auto f1 = converters().find(ID);
	assert( f1 != converters().end() );
	const auto f2 = f1->second.find( ID );
	assert( f2 != f1->second.end() );
	return f2->second->create();//trivial conversion to itself should always be there
}

Value Value::copyByID(size_t ID) const{
	const Converter &conv = getConverterTo( ID );
	Value to;

	if ( conv ) {
		switch ( conv->generate( *this, to ) ) {
		    case cPosOverflow:
			    LOG( Runtime, error ) << "Positive overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap().at(ID) ) << ".";
			    break;
		    case cNegOverflow:
			    LOG( Runtime, error ) << "Negative overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap().at(ID) ) << ".";
			    break;
		    case cInRange:
			    break;
		}

		return to; // return the generated Value-Object
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap().at(ID) );
		return createByID(ID); // return an empty Reference
	}
}

bool Value::fitsInto(size_t ID) const { //@todo find a better way to do this
	const Converter &conv = getConverterTo( ID );
	Value to = createByID(ID);

	if ( conv ) {
		return ( conv->generate( *this, to ) ==  cInRange );
	} else {
		LOG( Runtime, info )
		    << "I dont know any conversion from "
		    << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap().at(ID) );
		return false; // return an empty Reference
	}
}

bool Value::convert(const Value &from, Value &to) {
	const Converter &conv = from.getConverterTo( to.index() );

	if ( conv ) {
		switch ( conv->convert( from, to ) ) {
		    case cPosOverflow:
			    LOG( Runtime, error ) << "Positive overflow when converting " << from.toString( true ) << " to " << to.typeName() << ".";
			    break;
		    case cNegOverflow:
			    LOG( Runtime, error ) << "Negative overflow when converting " << from.toString( true ) << " to " << to.typeName() << ".";
			    break;
		    case cInRange:
			    return true;
		}
	} else {
		LOG( Runtime, error )
		    << "I don't know any conversion from "
		    << MSubject( from.toString( true ) ) << " to " << MSubject( to.typeName() );
	}

	return false;
}

bool Value::isFloat() const
{
	return std::visit(
	            [](auto ptr){
		return std::is_floating_point_v<decltype(ptr)>;
	},static_cast<const ValueTypes&>(*this)
	);
}

bool Value::isInteger() const
{
	return std::visit(
	            [](auto ptr){
		return std::is_integral_v<decltype(ptr)>;
	},static_cast<const ValueTypes&>(*this)
	);
}

bool Value::isValid() const{return !ValueTypes::valueless_by_exception();}

bool Value::gt(const Value &ref )const {
	return std::partial_ordering::greater == converted_three_way_compare(ref);
}
bool Value::lt(const Value &ref )const {
	return std::partial_ordering::less == converted_three_way_compare(ref);
}
bool Value::eq(const Value &ref )const {
	return converted_equal_compare(ref);
}


bool Value::apply(const isis::util::Value& other){
	return convert(other,*this);
}
std::ostream &operator<<(std::ostream &out, const Value &s)
{
	return s.print(false,out);
}
std::partial_ordering Value::operator<=>(const Value &rhs) const
{
	auto visitor=[this](auto &&ptr)->std::partial_ordering{
		typedef std::remove_cvref_t<decltype(ptr)> r_type;
		if constexpr(_internal::three_way_comparable_non_value<r_type>)
			return _internal::static_three_way_compare(*this,ptr);
		else
			LOG(Runtime,error) << "Cannot compare " << util::typeName<r_type>();
		return std::partial_ordering::unordered;
	};
	return std::visit(visitor,static_cast<const ValueTypes&>(rhs));
}
bool Value::operator==(const Value &rhs) const
{
	auto visitor=[this](auto &&ptr)->bool{
		typedef std::remove_cvref_t<decltype(ptr)> r_type;
		if constexpr(_internal::equal_comparable_non_value<r_type>)
			return _internal::static_equal_compare(*this,ptr);
		else
			LOG(Runtime,error) << "Cannot equal compare " << util::typeName<r_type>();
		return false;
	};
	return std::visit(visitor,static_cast<const ValueTypes&>(rhs));
}

std::partial_ordering Value::converted_three_way_compare(const Value &v)const
{
	return _internal::converted_compare(
		[](const Value &lhs,const Value &rhs){ assert(lhs.typeID()==rhs.typeID());return lhs<=>rhs;},
		*this,v
	);
}
bool Value::converted_equal_compare(const Value &v)const
{
	auto equal = [](const Value &lhs,const Value &rhs)->std::partial_ordering
	{
		assert(lhs.typeID()==rhs.typeID()); // if both are the same type we can refer to std::variant<>::operator==
		if(static_cast<const ValueTypes&>(lhs)==static_cast<const ValueTypes&>(rhs))
			return std::partial_ordering::equivalent;
		else
			return std::partial_ordering::unordered;
	};
	return _internal::converted_compare(equal,*this,v)==std::partial_ordering::equivalent;
}

Value::Value(const std::string_view &v): ValueTypes(std::string(v)){}
Value Value::operator+(const Value &ref) const{return _internal::arithmetic_op<std::plus<>>(*this,ref);}
Value Value::operator-(const Value &ref) const{return _internal::arithmetic_op<std::minus<>>(*this,ref);}
Value Value::operator*(const Value &ref) const{return _internal::arithmetic_op<std::multiplies<>>(*this,ref);}
Value Value::operator/(const Value &ref) const{return _internal::arithmetic_op<std::divides<>>(*this,ref);}

Value Value::operator+=(const Value &ref){return *this=_internal::arithmetic_op<std::plus<>>(*this,ref);}
Value Value::operator-=(const Value &ref){return *this=_internal::arithmetic_op<std::minus<>>(*this,ref);}
Value Value::operator*=(const Value &ref){return *this=_internal::arithmetic_op<std::multiplies<>>(*this,ref);}
Value Value::operator/=(const Value &ref){return *this=_internal::arithmetic_op<std::divides<>>(*this,ref);}

}
std::partial_ordering std::operator<=>(const isis::util::duration &lhs, const isis::util::duration &rhs)
{
	return lhs.count() <=> rhs.count();
}
std::partial_ordering std::operator<=>(const isis::util::timestamp &lhs, const isis::util::timestamp &rhs)
{
	return lhs.time_since_epoch() <=> rhs.time_since_epoch();
}
