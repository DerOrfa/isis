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
PropertyValue &PropertyValue::operator=(PropertyValue&& other) noexcept{
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
		return {"\u2205"};//utf8 for empty set
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
		LOG( Debug, error ) << "Interpretation of " << err << " as " << util::getTypeMap().at(dstID) << " failed. Keeping old type.";
		return false;
	} else {
		container.swap(ret.container);
		return true;
	}
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
size_t PropertyValue::explode(size_t factor, const std::function<Value(const Value &)>& op)
{
	for(auto e=container.begin();e!=container.end();){
		for(size_t i=0;i<factor;i++){
			container.insert(e,op(*e));
		}
		e=container.erase(e);
	}
	return container.size();
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

PropertyValue PropertyValue::plus( const PropertyValue& ref ) const{
	return operator_impl<std::plus<>>(ref);
}
PropertyValue PropertyValue::plus( const Value& ref ) const{
	return operator_impl<std::plus<>>(ref);
}

PropertyValue PropertyValue::minus( const PropertyValue& ref ) const{
	return operator_impl<std::minus<>>(ref);
}
PropertyValue PropertyValue::minus( const Value& ref ) const{
	return operator_impl<std::minus<>>(ref);
}

PropertyValue PropertyValue::multiply( const PropertyValue& ref ) const{
	return operator_impl<std::multiplies<>>(ref);
}
PropertyValue PropertyValue::multiply( const Value& ref ) const{
	return operator_impl<std::multiplies<>>(ref);
}

PropertyValue PropertyValue::divide( const PropertyValue& ref ) const{
	return operator_impl<std::divides<>>(ref);
}
PropertyValue PropertyValue::divide( const Value& ref ) const{
	return operator_impl<std::divides<>>(ref);
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
	for(const auto& other:ref.container)
		ret&=(mine_it++)->gt(other);
	return ret;
}
bool PropertyValue::lt( const PropertyValue& ref ) const{
	bool ret=true;
	if(ref.isEmpty() || ref.size()!=size())
		return false;
	auto mine_it=container.begin();
	for(const auto& other:ref.container)
		ret&=(mine_it++)->lt(other);
	return ret;
}

PropertyValue& PropertyValue::operator +=( const Value &second ){front()+=second;return *this;}
PropertyValue& PropertyValue::operator -=( const Value &second ){front()-=second;return *this;}
PropertyValue& PropertyValue::operator *=( const Value &second ){front()*=second;return *this;}
PropertyValue& PropertyValue::operator /=( const Value &second ){front()/=second;return *this;}

PropertyValue PropertyValue::operator+(const Value &second) const{return front()+second;}
PropertyValue PropertyValue::operator-(const Value &second) const{return front()-second;}
PropertyValue PropertyValue::operator*(const Value &second) const{return front()*second;}
PropertyValue PropertyValue::operator/(const Value &second) const{return front()/second;}

bool PropertyValue::operator<(const isis::util::PropertyValue& y) const{return lt(y);}
std::ostream &operator<<(std::ostream &out, const PropertyValue &s){
	return out<<s.toString(false);//should be false as this will be used implicitly in a lot of cases
}
template<typename OP>
PropertyValue PropertyValue::operator_impl(const PropertyValue &rhs) const
{
	PropertyValue ret;
	auto end=container.begin();
	if(rhs.size()<size()){
		LOG(Runtime,warning)
			<< "The PropertyValue at the left is loger than the one on the right. Will run operation "
			<< typeid(OP).name() << " only " << rhs.size() << " times";
		std::advance(end,rhs.size());
	} else
		end=container.end();

	LOG_IF(rhs.size()>size(),Runtime,warning)
		<< "The PropertyValue at the left is shorter than the one on the right. Will run operation "
		<< typeid(OP).name() << " only " << size() << " times";

	auto ret_it= std::inserter(ret.container,ret.container.begin());
	std::transform(container.begin(),container.end(),rhs.container.begin(),ret_it,OP{});
	return ret;
}
template<typename OP>
PropertyValue PropertyValue::operator_impl(const Value &rhs) const
{
PropertyValue ret;
auto ret_it= std::inserter(ret.container,ret.container.begin());
std::transform(container.begin(),container.end(),ret_it,std::bind(OP{},std::placeholders::_1,rhs));
return ret;
}
void PropertyValue::resize(size_t size, const Value& insert)
{
	const auto mysize=container.size();
	LOG_IF(size-mysize>1,Debug, warning ) << "Growing a Property. You should avoid this, as it is expensive.";
	if(mysize<size){ // grow
		container.insert(container.end(), size-mysize, insert);
	} else if(mysize>size){ // shrink
		auto erasestart=std::next(container.begin(),size);
		erase(erasestart,container.end());
	}
}
std::string PropertyValue::toString() const
{
	return toString(false);
}

}
/// @endcond _internal
