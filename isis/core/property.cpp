//
// C++ Implementation: property
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "property.hpp"
#include "stringop.hpp"

namespace isis::util
{

bool &PropertyValue::needed() { return m_needed;}
bool PropertyValue::isNeeded()const { return m_needed;}


bool PropertyValue::operator== ( const util::PropertyValue &second )const
{
	return !isEmpty() && !second.isEmpty() && container==second.container;
}
bool PropertyValue::operator!= ( const util::PropertyValue &second )const
{
	return !isEmpty() && !second.isEmpty() && container!=second.container;
}

bool PropertyValue::operator== ( const Value &second )const
{
	return size()==1 && front()==second;
}
bool PropertyValue::operator!=( const Value& second ) const
{
	return size()==1 && front()!=second;
}


PropertyValue::PropertyValue ( ) : m_needed( false ) {}
PropertyValue &PropertyValue::operator=(const PropertyValue& other){
	container=other.container;
	return *this;
}
PropertyValue &PropertyValue::operator=(PropertyValue&& other){
	container.swap(other.container);
	return *this;
}


PropertyValue PropertyValue::copyByID( short unsigned int ID ) const
{
	PropertyValue ret;
	for(auto &v:*this){
		ret.push_back(v.copyByID(ID));
	}
	return ret;
}

std::string PropertyValue::toString( bool labeled )const
{
	if(container.empty()){
		return std::string("\u2205");//utf8 for empty set
	} else if(size()==1)
		return front().toString(labeled);
	else{
		std::string ret=listToString(begin(),end(),",","[","]");
		if(labeled && !isEmpty())
			ret+="("+getTypeName()+"["+std::to_string(size())+"])";
		return ret;
	}
}
bool PropertyValue::isEmpty() const{return container.empty();}

const Value &PropertyValue::operator()() const{return front();}
Value &PropertyValue::operator()(){return front();}

void PropertyValue::push_back( const Value& ref ){insert(end(), ref);}
void PropertyValue::push_back( Value&& ref ){insert(end(), ref);}

PropertyValue::iterator PropertyValue::insert( iterator at, const Value& ref ){
	LOG_IF(!isEmpty() && getTypeID()!=ref.typeID(),Debug,error) << "Inserting value of inconsistent type " << MSubject(ref.toString(true)) << " into " << MSubject(*this);
	return container.insert(at,ref );
}

void PropertyValue::transfer(isis::util::PropertyValue::iterator at, PropertyValue& ref)
{
	if(ref.isEmpty()){
		LOG(Debug,error) << "Not transferring empty Property";
	} else {
		LOG_IF(!isEmpty() && getTypeID()!=ref.getTypeID(),Debug,error) << "Inserting value of inconsistent type " << MSubject(ref.toString(true)) << " into " << MSubject(*this);
		container.splice(at,ref.container );
	}
}

void PropertyValue::transfer(PropertyValue& ref, bool overwrite)
{
	if(ref.isEmpty()){
		LOG(Debug,error) << "Not transferring empty Property";
	} else {
		if(isEmpty() || overwrite){
			container.clear();
			swap(ref);
		} else
			LOG(Debug,warning) << "Not transferring " << MSubject(ref.toString(true)) <<  " into non empty " << MSubject(*this);
	}
}
void PropertyValue::swap(PropertyValue &src)
{
	container.swap(src.container);
}

bool PropertyValue::transform(uint16_t dstID)
{
	PropertyValue ret,err;
	for(const Value& ref : container){
        #pragma message "no error handling for failed convert"
		ret.push_back(ref.copyByID( dstID ));
	}

	if(!err.isEmpty()){
		LOG( Debug, error ) << "Interpretation of " << err << " as " << util::getTypeMap()[dstID] << " failed. Keeping old type.";
		return false;
	} else {
		container.swap(ret.container);
		return true;
	}
	return container.size();
}


Value& PropertyValue::at(size_t n ){auto it=container.begin(); std::advance(it, n);return *it;}
const Value& PropertyValue::at(size_t n ) const{auto it=container.begin(); std::advance(it, n);return *it;}

Value& PropertyValue::operator[](size_t n ){return at(n);}
const Value& PropertyValue::operator[](size_t n ) const{return at(n);}

PropertyValue::iterator PropertyValue::begin(){return container.begin();}
PropertyValue::const_iterator PropertyValue::begin() const{return container.begin();}
PropertyValue::iterator PropertyValue::end(){return container.end();}
PropertyValue::const_iterator PropertyValue::end() const{return container.end();}

PropertyValue::iterator PropertyValue::erase( size_t at ){
	auto i=begin();std::advance(i,at);
	return container.erase(i);
}
PropertyValue::iterator PropertyValue::erase( iterator first, iterator last ){return container.erase(first,last);}

Value& PropertyValue::front(){
	LOG_IF(size()>1,Debug,warning) << "Doing single value operation on a multi value Property";
	LOG_IF(isEmpty(),Debug,error) << "Doing single value operation on an empty Property, exception ahead ..";
	return container.front();

}
const Value& PropertyValue::front() const{
	LOG_IF(size()>1,Debug,warning) << "Doing single value operation on a multi value Property (" << util::listToString(begin(),end()) << ")";
	LOG_IF(isEmpty(),Debug,error) << "Doing single value operation on an empty Property, exception ahead ..";
	return container.front();
}

size_t PropertyValue::size() const{return container.size();}

std::vector< PropertyValue > PropertyValue::splice( const size_t len )
{
	assert(len);
	size_t remain=size();//we use this to keep track of the number of remaining elements
	std::vector<PropertyValue> ret(ceil(double(remain)/len));

	for(PropertyValue &dst:ret){
		auto e=container.begin();std::advance(e,std::min(len,remain));//move either len elements, or all that is left
		dst.container.splice(dst.begin(),container,container.begin(),e);
		remain-=dst.container.size();
		assert(size()==remain);
	}
	assert(isEmpty());
	return ret;
}


// Value hooks
bool PropertyValue::fitsInto( short unsigned int ID ) const{
	return begin()->fitsInto(ID);//use begin() instead of front() to avoid warning about single value operation on a multi value Property
}

std::string PropertyValue::getTypeName() const{
	LOG_IF(isEmpty(),Debug,error) << "Doing getTypeName on an empty PropertyValue will raise an exception.";
	return begin()->typeName();//use begin() instead of front() to avoid warning about single value operation on a multi value Property
}
short unsigned int PropertyValue::getTypeID() const{
	LOG_IF(isEmpty(),Debug,error) << "Doing getTypeID on an empty PropertyValue will raise an exception.";
	return begin()->typeID();//use begin() instead of front() to avoid warning about single value operation on a multi value Property
}

PropertyValue& PropertyValue::add( const PropertyValue& ref ){
	LOG_IF(ref.isEmpty(),Debug,error) << "Adding an empty property, won't do anything";
	auto mine_it=container.begin();
	auto other_it=ref.container.begin();

	for(;mine_it != container.end() && other_it != ref.container.end();++mine_it,++other_it)
		mine_it->add(*other_it);
	return *this;
}
PropertyValue& PropertyValue::subtract(const PropertyValue& ref ){
	LOG_IF(ref.isEmpty(),Debug,error) << "Subtracting an empty property, won't do anything";
	auto mine_it=container.begin();
	auto other_it=ref.container.begin();

	for(;mine_it != container.end() && other_it != ref.container.end();++mine_it,++other_it)
		mine_it->subtract(*other_it);
	return *this;
}
PropertyValue& PropertyValue::multiply_me( const PropertyValue& ref ){
	LOG_IF(ref.isEmpty(),Debug,error) << "Multiplying with an empty property, won't do anything";
	auto mine_it=container.begin();
	auto other_it=ref.container.begin();

	for(;mine_it != container.end() && other_it != ref.container.end();++mine_it,++other_it)
		mine_it->multiply_me(*other_it);
	return *this;
}
PropertyValue& PropertyValue::divide_me( const PropertyValue& ref ){
	LOG_IF(ref.isEmpty(),Debug,error) << "Dividing by an empty property, won't do anything";
	auto mine_it=container.begin();
	auto other_it=ref.container.begin();

	for(;mine_it != container.end() && other_it != ref.container.end();++mine_it,++other_it)
		mine_it->divide_me(*other_it);
	return *this;
}

PropertyValue PropertyValue::plus( const PropertyValue& ref ) const{
	PropertyValue ret(*this);
	ret.add(ref);
	return ret;
}
PropertyValue PropertyValue::minus( const PropertyValue& ref ) const{
	PropertyValue ret(*this);
	ret.subtract(ref);
	return ret;
}
PropertyValue PropertyValue::multiply( const PropertyValue& ref ) const{
	PropertyValue ret(*this);
	ret.multiply_me(ref);
	return ret;
}
PropertyValue PropertyValue::divide( const PropertyValue& ref ) const{
	PropertyValue ret(*this);
	ret.divide_me(ref);
	return ret;
}

//@todo maybe use std::transform_reduce
bool PropertyValue::eq( const PropertyValue& ref ) const{
	bool ret=true;
	if(ref.isEmpty() || ref.size()!=size())
		return false;
	auto mine_it=container.begin();
	for(const Value &other:ref.container)
		ret&=(mine_it++)->eq(other);
	return ret;
}
bool PropertyValue::gt( const PropertyValue& ref ) const{
	bool ret=true;
	if(ref.isEmpty() || ref.size()!=size())
		return false;
	auto mine_it=container.begin();
	for(auto other:ref.container)
		ret&=(mine_it++)->gt(other);
	return ret;
}
bool PropertyValue::lt( const PropertyValue& ref ) const{
	bool ret=true;
	if(ref.isEmpty() || ref.size()!=size())
		return false;
	auto mine_it=container.begin();
	for(auto other:ref.container)
		ret&=(mine_it++)->lt(other);
	return ret;
}

PropertyValue& PropertyValue::operator +=( const Value &second ){front().add(second);return *this;}
PropertyValue& PropertyValue::operator -=( const Value &second ){ front().subtract(second);return *this;}
PropertyValue& PropertyValue::operator *=( const Value &second ){front().multiply_me(second);return *this;}
PropertyValue& PropertyValue::operator /=( const Value &second ){front().divide_me(second);return *this;}

bool PropertyValue::operator<(const isis::util::PropertyValue& y) const{return lt(y);}
std::ostream &operator<<(std::ostream &out, const PropertyValue &s){return out<<s.toString(true);}

}
/// @endcond _internal
