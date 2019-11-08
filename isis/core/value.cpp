#include "value.hpp"

#include <sstream>

namespace isis::util{

namespace _internal{

}

ValueNew::ValueNew(const ValueTypes &v):ValueTypes(v){print(true,std::cout<<"Copy created ")<< std::endl;}

ValueNew::ValueNew(ValueTypes &&v):ValueTypes(v){print(true,std::cout<<"Move created ")<< std::endl;}
ValueNew::ValueNew():ValueTypes(){}

std::string ValueNew::toString(bool with_typename)const{
	std::stringstream o;
	print(with_typename,o);
	return o.str();
}

std::string ValueNew::typeName() const {
	return std::visit(_internal::name_visitor(),static_cast<const ValueTypes&>(*this));
}

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
				LOG( Runtime, error ) << "Positive overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap( true, false )[ID] ) << ".";
				break;
			case boost::numeric::cNegOverflow:
				LOG( Runtime, error ) << "Negative overflow when converting " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap( true, false )[ID] ) << ".";
				break;
			case boost::numeric::cInRange:
				break;
		}

		return to; // return the generated Value-Object - wrapping it into Reference
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << MSubject( toString( true ) ) << " to " << MSubject( getTypeMap( true, false )[ID] );
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
			<< MSubject( toString( true ) ) << " to " << MSubject( getTypeMap( true, false )[ID] );
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

bool TypeNew::gt( const TypeNew &ref )const {return operatorVisitor(_internal::type_greater<TYPE>(),ref,false );}
bool lt( const ValueBase &ref )const {return operatorWrapper(_internal::type_less<TYPE>(),   ref,false );}
bool eq( const ValueBase &ref )const {return operatorWrapper(_internal::type_eq<TYPE>(),     ref, false );}



}
