#include "value.hpp"
#include "valuearray_iterator.hpp"

#include <sstream>

namespace isis::util{

Value::Value(const ValueTypes &v): ValueTypes(v){
	LOG(Debug,verbose_info) << "Value copy created from " << v;
}

Value::Value(ValueTypes &&v): ValueTypes(v){
	LOG(Debug,verbose_info) << "Value move created from " << v;
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
		    case boost::numeric::cPosOverflow:
			    LOG( Runtime, error ) << "Positive overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] ) << ".";
			    break;
		    case boost::numeric::cNegOverflow:
			    LOG( Runtime, error ) << "Negative overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] ) << ".";
			    break;
		    case boost::numeric::cInRange:
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
		return ( conv->generate( *this, to ) ==  boost::numeric::cInRange );
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
		    case boost::numeric::cPosOverflow:
			    LOG( Runtime, error ) << "Positive overflow when converting " << from.toString( true ) << " to " << to.typeName() << ".";
			    break;
		    case boost::numeric::cNegOverflow:
			    LOG( Runtime, error ) << "Negative overflow when converting " << from.toString( true ) << " to " << to.typeName() << ".";
			    break;
		    case boost::numeric::cInRange:
			    return true;
			    break;
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
	auto op = [&](auto ptr){
		return operatorWrapper(_internal::type_greater<decltype(ptr)>(),ref,false );
	};
	return std::visit(op,static_cast<const ValueTypes&>(*this));
}
bool Value::lt(const Value &ref )const {
	auto op=[&](auto ptr){
		return operatorWrapper(_internal::type_less<decltype(ptr)>(),ref,false );
	};
	return std::visit(op,static_cast<const ValueTypes&>(*this));
}
bool Value::eq(const Value &ref )const {
	auto op=[&](auto ptr){
		return operatorWrapper(_internal::type_eq<decltype(ptr)>(),ref,false );
	};
	return std::visit(op,static_cast<const ValueTypes&>(*this));
}

Value Value::plus(const Value &ref )const{return Value(*this).add(ref);}
Value Value::minus(const Value &ref )const{return Value(*this).substract(ref);}
Value Value::multiply(const Value &ref )const{return Value(*this).multiply_me(ref);}
Value Value::divide(const Value &ref )const{return Value(*this).divide_me(ref);}

Value& Value::add(const Value &ref )
{
	auto op=[&](auto ptr)->Value&{
		return operatorWrapper_me(_internal::type_plus<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

Value& Value::substract(const Value &ref )
{
	auto op=[&](auto ptr)->Value&{
		return operatorWrapper_me(_internal::type_minus<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

Value& Value::multiply_me(const Value &ref )
{
	auto op=[&](auto ptr)->Value&{
		return operatorWrapper_me(_internal::type_mult<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

Value& Value::divide_me(const Value &ref )
{
	auto op=[&](auto ptr)->Value&{
		return operatorWrapper_me(_internal::type_div<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

bool Value::apply(const isis::util::Value& other){
	return convert(other,*this);
}

}
