#include "valuearray_iterator.hpp"

/// @cond _internal
namespace isis::data::_internal
{
template<> GenericValueIterator<true>::reference GenericValueIterator<true>::operator*() const
{
	assert( getValueFunc );
	return ConstValueAdapter( p, getValueFunc );
}
template<> GenericValueIterator<false>::reference GenericValueIterator<false>::operator*() const
{
	assert( getValueFunc );
	return WritingValueAdapter( p, getValueFunc, setValueFunc, byteSize );
}

ConstValueAdapter::ConstValueAdapter( const uint8_t *const _p, Getter _getValueFunc ):getter(_getValueFunc), p( _p ) {}
bool ConstValueAdapter::operator==( const util::ValueNew &val )const {return getter(p).eq( val );}
bool ConstValueAdapter::operator!=( const util::ValueNew &val )const {return !operator==( val );}
bool ConstValueAdapter::operator==( const ConstValueAdapter &val )const {return getter(p).eq( util::ValueNew(val) );}
bool ConstValueAdapter::operator!=( const ConstValueAdapter &val )const {return !operator==( val );}

bool ConstValueAdapter::operator<( const util::ValueNew &val )const {return getter(p).lt( val );}
bool ConstValueAdapter::operator>( const util::ValueNew &val )const {return getter(p).gt( val );}
bool ConstValueAdapter::operator<( const ConstValueAdapter &val )const {return getter(p).lt( util::ValueNew(val) );}
bool ConstValueAdapter::operator>( const ConstValueAdapter &val )const {return getter(p).gt( util::ValueNew(val) );}

const std::unique_ptr<util::ValueNew> ConstValueAdapter::operator->() const{return std::make_unique<util::ValueNew>(getter(p));}
const std::string ConstValueAdapter::toString( bool label ) const{ return getter(p).toString(label);}

WritingValueAdapter::WritingValueAdapter( uint8_t*const _p, ConstValueAdapter::Getter _getValueFunc, ConstValueAdapter::Setter _setValueFunc, size_t _byteSize )
: ConstValueAdapter( _p, _getValueFunc ), setValueFunc( _setValueFunc ), byteSize(_byteSize) {}
WritingValueAdapter WritingValueAdapter::operator=( const util::ValueNew& val )
{
	assert( setValueFunc );
	setValueFunc( const_cast<uint8_t * const>( p ), val );
	return *this;
}

void WritingValueAdapter::swapwith(const WritingValueAdapter& other )const
{
	LOG_IF(setValueFunc != other.setValueFunc,Debug,error) << "Swapping ValueArray iterators with different set function. This is very likely an error";
	assert(setValueFunc == other.setValueFunc);
	assert(byteSize == other.byteSize);
	uint8_t * const a=const_cast<uint8_t * const>( p );
	uint8_t * const b=const_cast<uint8_t * const>( other.p );
	
	switch(byteSize){ // shortcuts for register sizes
		case 2:std::swap(*(uint16_t*)a,*(uint16_t*)b);break;
		case 4:std::swap(*(uint32_t*)a,*(uint32_t*)b);break;
		case 8:std::swap(*(uint64_t*)a,*(uint64_t*)b);break;
		default:{ // anything else
			for(size_t i=0;i<byteSize;i++)
				std::swap(a[i],b[i]);
		}
	}
}

} // namespace _internal
/// @endcond _internal


void std::swap(const isis::data::_internal::WritingValueAdapter& a,const isis::data::_internal::WritingValueAdapter& b )
{
	a.swapwith(b);
}
