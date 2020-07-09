#include "value.hpp"
#include "valuearray_iterator.hpp"

#include <sstream>

namespace isis::util{

ValueNew::ValueNew(const ValueTypes &v):ValueTypes(v){
	LOG(Debug,verbose_info) << "Value copy created from " << v;
}

ValueNew::ValueNew(ValueTypes &&v):ValueTypes(v){
	LOG(Debug,verbose_info) << "Value move created from " << v;
}

std::string ValueNew::toString(bool with_typename)const{
	std::stringstream o;
	print(with_typename,o);
	return o.str();
}

std::string ValueNew::typeName() const {
	return std::visit(_internal::name_visitor(),static_cast<const ValueTypes&>(*this));
}
size_t ValueNew::typeID()const{return index();}

const _internal::ValueConverterMap &ValueNew::converters(){
	static _internal::ValueConverterMap ret;
	return ret;
}

const ValueNew::Converter &ValueNew::getConverterTo(unsigned short ID) const {
	const auto f1 = converters().find( index() );
	assert( f1 != converters().end() );
	const auto f2 = f1->second.find( ID );
	assert( f2 != f1->second.end() );
	return f2->second;
}

ValueNew ValueNew::createByID(unsigned short ID) {
	const auto f1 = converters().find(ID);
	assert( f1 != converters().end() );
	const auto f2 = f1->second.find( ID );
	assert( f2 != f1->second.end() );
	return f2->second->create();//trivial conversion to itself should always be there
}

ValueNew ValueNew::copyByID(unsigned short ID) const{
	const Converter &conv = getConverterTo( ID );
	ValueNew to;

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

		return to; // return the generated Value-Object - wrapping it into Reference
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] );
		return createByID(ID); // return an empty Reference
	}
}

bool ValueNew::fitsInto(unsigned short ID) const { //@todo find a better way to do this
	const Converter &conv = getConverterTo( ID );
	ValueNew to = createByID(ID);

	if ( conv ) {
		return ( conv->generate( *this, to ) ==  boost::numeric::cInRange );
	} else {
		LOG( Runtime, info )
		    << "I dont know any conversion from "
		    << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap()[ID] );
		return false; // return an empty Reference
	}
}

bool ValueNew::convert(const ValueNew &from, ValueNew &to) {
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

bool ValueNew::isFloat() const
{
	return std::visit(
	            [](auto ptr){
		return std::is_floating_point_v<decltype(ptr)>;
	},static_cast<const ValueTypes&>(*this)
	);
}

bool ValueNew::isInteger() const
{
	return std::visit(
	            [](auto ptr){
		return std::is_integral_v<decltype(ptr)>;
	},static_cast<const ValueTypes&>(*this)
	);
}

bool ValueNew::isValid() const{return !ValueTypes::valueless_by_exception();}

bool ValueNew::gt( const ValueNew &ref )const {
	auto op = [&](auto ptr){
		return operatorWrapper(_internal::type_greater<decltype(ptr)>(),ref,false );
	};
	return std::visit(op,static_cast<const ValueTypes&>(*this));
}
bool ValueNew::lt( const ValueNew &ref )const {
	auto op=[&](auto ptr){
		return operatorWrapper(_internal::type_less<decltype(ptr)>(),ref,false );
	};
	return std::visit(op,static_cast<const ValueTypes&>(*this));
}
bool ValueNew::eq( const ValueNew &ref )const {
	auto op=[&](auto ptr){
		return operatorWrapper(_internal::type_eq<decltype(ptr)>(),ref,false );
	};
	return std::visit(op,static_cast<const ValueTypes&>(*this));
}

ValueNew& ValueNew::add( const ValueNew &ref )
{
	auto op=[&](auto ptr)->ValueNew&{
		return operatorWrapper_me(_internal::type_plus<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

ValueNew& ValueNew::substract( const ValueNew &ref )
{
	auto op=[&](auto ptr)->ValueNew&{
		return operatorWrapper_me(_internal::type_minus<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

ValueNew& ValueNew::multiply_me( const ValueNew &ref )
{
	auto op=[&](auto ptr)->ValueNew&{
		return operatorWrapper_me(_internal::type_mult<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

ValueNew& ValueNew::divide_me( const ValueNew &ref )
{
	auto op=[&](auto ptr)->ValueNew&{
		return operatorWrapper_me(_internal::type_div<decltype(ptr)>(), ref );
	};
	return std::visit(op,static_cast<ValueTypes&>(*this));
}

bool ValueNew::apply(const isis::util::ValueNew& other){
	return convert(other,*this);
}

ValueNew::ValueNew(const data::_internal::ConstValueAdapter& v):ValueNew(v.operator->()){}

}
