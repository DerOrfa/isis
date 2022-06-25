#include "value.hpp"
#include "value_converter.hpp"

#include <sstream>

namespace isis::util{

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
			    LOG( Runtime, error ) << "Positive overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] ) << ".";
			    break;
		    case cNegOverflow:
			    LOG( Runtime, error ) << "Negative overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] ) << ".";
			    break;
		    case cInRange:
			    break;
		}

		return to; // return the generated Value-Object
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] );
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
		    << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] );
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

Value Value::operator+(const std::string &rhs) const{return this->as<std::string>()+rhs;}
Value Value::operator+(const duration &rhs) const{return chrono_math<std::plus<>>(rhs);}
Value Value::operator-(const duration &rhs) const{return chrono_math<std::minus<>>(rhs);}

bool Value::gt(const Value &ref )const {
	return std::partial_ordering::greater == converted_three_way_compare(ref);
}
bool Value::lt(const Value &ref )const {
	return std::partial_ordering::less == converted_three_way_compare(ref);
}
bool Value::eq(const Value &ref )const {
	return std::partial_ordering::equivalent == converted_three_way_compare(ref);
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
		if constexpr(three_way_comparable_non_value<r_type>)
			return this->operator<=>(ptr);
		else
			LOG(Runtime,error) << "Cannot compare " << util::typeName<r_type>();
		return std::partial_ordering::unordered;
	};
	return std::visit(visitor,static_cast<const ValueTypes&>(rhs));
}
std::partial_ordering Value::converted_three_way_compare(const Value &v)const
{
	const Converter &conv = v.getConverterTo( typeID() );
	Value to = createByID(typeID());

	if ( conv ) {
		switch ( conv->convert( v, to ) ){
		case cInRange:return this->operator<=>(to);
		case cNegOverflow: return std::partial_ordering::greater;//v is so small it can't be represented in my type
		case cPosOverflow: return std::partial_ordering::less;//v is so big it can't be represented in my type
		}
	} else {
		LOG( Runtime, info )
			<< "I can't compare " << MSubject( toString( true ) ) << " to " << MSubject( v.toString(true) )
			<< " as " << v.typeName() << " can't be converted into " << typeName();
	}
	return std::partial_ordering::unordered;
}

Value::Value(const std::string_view &v): ValueTypes(std::string(v)){}
Value Value::operator+=(const std::string &rhs){return *this=this->as<std::string>()+rhs;}
Value Value::operator+(const Value &ref) const{return arithmetic_op<std::plus<>>(ref);}
Value Value::operator-(const Value &ref) const{return arithmetic_op<std::minus<>>(ref);}
Value Value::operator*(const Value &ref) const{return arithmetic_op<std::multiplies<>>(ref);}
Value Value::operator/(const Value &ref) const{return arithmetic_op<std::divides<>>(ref);}

Value Value::operator+=(const Value &ref){return *this=arithmetic_op<std::plus<>>(ref);}
Value Value::operator-=(const Value &ref){return *this=arithmetic_op<std::minus<>>(ref);}
Value Value::operator*=(const Value &ref){return *this=arithmetic_op<std::multiplies<>>(ref);}
Value Value::operator/=(const Value &ref){return *this=arithmetic_op<std::divides<>>(ref);}

}
