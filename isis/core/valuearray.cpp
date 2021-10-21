#include "valuearray.hpp"
#include "valuearray_converter.hpp"
#include "endianess.hpp"

namespace isis::data{

_internal::DelProxy::DelProxy(const ValueArray& master)
:std::shared_ptr<const void>(master.getRawAddress())
{
	assert(get());
	LOG( Debug, verbose_info ) << "Creating DelProxy for " << master.typeName() << " at " << get();
}
void _internal::DelProxy::operator()(const void* at)
{
	LOG( Debug, verbose_info )
	<< "Deletion for " << this->get() << " called from proxy at offset "
	<< static_cast<const uint8_t *>( at ) - static_cast<const uint8_t *>( get() )
	<< ", current use_count: " << this->use_count();
	this->reset();//actually not needed, but we keep it here to keep obfuscation low
}

std::shared_ptr<const void> ValueArray::getRawAddress(size_t offset) const{
	std::shared_ptr<const void> b_ptr = std::visit(
		[](const auto &p){return std::static_pointer_cast<const void>(p);},
		static_cast<const ArrayTypes&>(*this)
		);
	if( offset ) {
		_internal::DelProxy proxy( *this );
		const uint8_t* o_ptr=std::static_pointer_cast<const uint8_t>(b_ptr).get();
		return std::shared_ptr<const void>( o_ptr+offset, proxy );
	} else
		return b_ptr;
}

std::shared_ptr<void> ValueArray::getRawAddress(size_t offset) { // use the const version and cast away the const
	const std::shared_ptr<const void> ptr=const_cast<const ValueArray*>(this)->getRawAddress(offset );
	return std::const_pointer_cast<void>( ptr );
}

std::string ValueArray::typeName() const{
	return std::visit(_internal::arrayname_visitor(),static_cast<const ArrayTypes&>(*this));
}

ValueArray::iterator ValueArray::begin() {
	return visit([](auto ptr)->iterator{
		typedef typename decltype(ptr)::element_type element_type;
		auto p = std::reinterpret_pointer_cast<std::byte>(ptr).get();
		return iterator(
			p, p, sizeof(element_type),
			&getValueFrom<element_type>, &setValueInto<element_type>
			);
	});
}
ValueArray::const_iterator ValueArray::begin() const {
	return const_cast<ValueArray*>(this)->begin();
}

ValueArray::iterator ValueArray::end() {
	return begin()+m_length;
}

ValueArray::const_iterator ValueArray::end() const {
	return begin()+m_length;
}

std::string ValueArray::toString(bool labeled) const {
	std::stringstream ret;
	ret << *this;
	return ret.str();
}

std::vector<ValueArray> ValueArray::splice(size_t size) const{
	if ( size >= getLength() ) {
		LOG( Debug, warning )
		<< "splicing data of the size " << getLength() << " up into blocks of the size " << size << " is kind of useless ...";
	}

	const size_t fullSplices = getLength() / size;
	const size_t lastSize = getLength() % size;//rest of the division - size of the last spliceAt
	const size_t splices = fullSplices + ( lastSize ? 1 : 0 );

	_internal::DelProxy proxy( *this );

	auto generator = [&](auto ptr){
		std::vector<ValueArray> ret(splices );

		for ( size_t i = 0; i < fullSplices; i++ )
			ret[i]=ValueArray(ptr.get() + i * size, size, proxy );

		if ( lastSize )
			ret.back()= ValueArray(ptr.get() + fullSplices * size, lastSize, proxy );

		return ret;
	};

	return std::visit(generator,static_cast<const ArrayTypes&>(*this));
}

scaling_pair ValueArray::getScalingTo(unsigned short typeID) const {
	return getScalingTo(typeID, getMinMax());
}
scaling_pair ValueArray::getScalingTo(unsigned short typeID, const std::pair<util::Value, util::Value> &minmax )const
{
	const Converter &conv = getConverterTo( typeID );

	if ( conv ) {
		return conv->getScaling( minmax.first, minmax.second );
	} else {
		LOG( Runtime, error )
		<< "I don't know any conversion from " << typeName() << " to " << util::getTypeMap( true )[typeID];
		return {};//return invalid scaling
	}
}

std::size_t ValueArray::bytesPerElem() const{
	return visit([](auto ptr){return sizeof(typename decltype(ptr)::element_type);});
}

void ValueArray::endianSwap() {
	const size_t len=m_length;
	if(bytesPerElem()>1){
		std::visit([len](auto ptr){
			data::endianSwapArray( ptr.get(), ptr.get()+len, ptr.get() );
			},static_cast<const ArrayTypes&>(*this));
	}

}

std::pair<isis::util::Value, isis::util::Value> ValueArray::getMinMax() const{
	if ( getLength() == 0 ) {
		LOG( Debug, error ) << "Skipping computation of min/max on an empty ValueArray";
		return std::pair<util::Value, util::Value>();
	} else {
		_internal::getMinMaxVisitor visitor(getLength());
		std::visit(visitor,static_cast<const ArrayTypes&>(*this));
		return visitor.minmax;
	}
}

const _internal::ValueArrayConverterMap & ValueArray::converters(){
	static const _internal::ValueArrayConverterMap map;
	return map;
}

ValueArray::ValueArray(): m_length(0)
{}

bool ValueArray::isValid() const{
	return index()!=std::variant_npos && std::visit([](auto ptr){return (bool)ptr;}, static_cast<const ArrayTypes&>(*this));
}

ValueArray ValueArray::copyByID(size_t ID, const scaling_pair &scaling) const
{
	const Converter &conv = getConverterTo( ID );

	if( conv ) {
		return conv->generate( *this, getScaling( scaling, ID ) );
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << typeName() << " to " << util::getTypeMap( true )[ID];
		return {}; // return an invalid array
	}
}
bool ValueArray::copyTo(ValueArray& dst, scaling_pair scaling) const
{
	const unsigned short dID = dst.getTypeID();
	const Converter &conv = getConverterTo( dID );

	if( conv ) {
		conv->convert( *this, dst, getScaling( scaling, dID ));
		return true;
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << toString( true ) << " to " << dst.typeName();
		return false;
	}
}

void ValueArray::copyRange(std::size_t start, std::size_t end, ValueArray& dst, std::size_t dst_start) const
{
	assert( start <= end );
	const size_t len = end - start + 1;
	LOG_IF(this->getTypeID()!=dst.getTypeID(), Debug, error )
	<< "Range copy into a ValueArray of different type is not supportet. Its " << dst.typeName() << " not " << typeName();

	if( end > getLength() ) {
		LOG( Runtime, error )
		<< "End of the range (" << end << ") is behind the end of this ValueArray (" << getLength() << ")";
	} else if( len + dst_start > dst.getLength() ) {
		LOG( Runtime, error )
		<< "End of the range (" << len + dst_start << ") is behind the end of the destination (" << dst.getLength() << ")";
	} else {
		std::shared_ptr<void> daddr = dst.getRawAddress();
		const std::shared_ptr<const void> saddr = getRawAddress();
		const size_t soffset = bytesPerElem() * start; //source offset in bytes
		const int8_t *const  src = ( int8_t * )saddr.get();
		const size_t doffset = bytesPerElem() * dst_start;//destination offset in bytes
		int8_t *const  dest = ( int8_t * )daddr.get();
		const size_t blength = len * bytesPerElem();//length in bytes
		memcpy( dest + doffset, src + soffset, blength );
	}
}
std::size_t ValueArray::getTypeID() const{
	return std::visit(
		[](auto ptr){return util::typeID<typename decltype(ptr)::element_type>();},
		static_cast<const ArrayTypes&>(*this)
		);
}

ValueArray ValueArray::convertByID(unsigned short ID, scaling_pair scaling) const{
	scaling = getScaling(scaling, ID);

	assert(scaling.valid );
	if( !scaling.isRelevant() && getTypeID() == ID ) { // if type is the same and scaling is 1/0
		return *this; //cheap copy
	} else {
		return copyByID( ID, scaling ); // convert into new
	}
}

ValueArray ValueArray::createByID(unsigned short ID, std::size_t len)
{
	auto f1 = converters().find( ID );
	_internal::ValueArrayConverterMap::mapped_type::const_iterator f2;
	ValueArray ret;
	// try to get a converter to convert the requested type into itself - they're there for all known types
	if( f1 != converters().end() && ( f2 = f1->second.find( ID ) ) != f1->second.end() ) {
		const ValueArrayConverterBase &conv = *( f2->second );
		ret=conv.create( len );
		LOG_IF(!ret.isValid(),Runtime,error) << "The created array is not valid, this is not going to end well..";
	} else {
		LOG( Debug, error ) << "There is no known array creator for " << util::getTypeMap()[ID];
	}
	return ret;
}

std::size_t ValueArray::useCount() const
{
	return std::visit([](auto ptr){return ptr.use_count();},static_cast<const ArrayTypes&>(*this));
}

ValueArray ValueArray::cloneToNew(std::size_t length) const
{
	return createByID(getTypeID(),length);
}

std::size_t ValueArray::compare(std::size_t start, std::size_t end, const ValueArray& dst, std::size_t dst_start) const
{
	assert( start <= end );
	size_t ret = 0;
	size_t _length = end - start;

	if ( dst.getTypeID() != getTypeID() ) {
		LOG( Debug, error )
		<< "Comparing to a ValueArray of different type(" << dst.typeName() << ", not " << typeName()
		<< "). Assuming all voxels to be different";
		return _length;
	}

	LOG_IF( end >= getLength(), Runtime, error )
	<< "End of the range (" << end << ") is behind the end of this ValueArray (" << getLength() << ")";
	LOG_IF( _length + dst_start >= dst.getLength(), Runtime, error )
	<< "End of the range (" << _length + dst_start << ") is behind the end of the destination (" << dst.getLength() << ")";

	// lock the memory, so we can mem-compare the elements (use uint8_t because some compilers do not like arith on void*)
	const std::shared_ptr<const uint8_t>
	src_s = std::static_pointer_cast<const uint8_t>( getRawAddress() ),
	dst_s = std::static_pointer_cast<const uint8_t>( dst.getRawAddress() );
	const uint8_t *src_p = src_s.get(), *dst_p = dst_s.get();
	const size_t el_size = bytesPerElem();

	for ( size_t i = start; i < end; i++ ) {
		if ( memcmp( src_p + ( i * el_size ), dst_p + ( i * el_size ), el_size ) != 0 )
			ret++;
	}

	return ret;
}

const ValueArray::Converter & ValueArray::getConverterFromTo(unsigned short fromID, unsigned short toID)
{
	const auto f1 = converters().find( fromID );
	LOG_IF( f1 == converters().end(), Debug, error ) << "There is no known conversion from " << util::getTypeMap()[fromID];
	const auto f2 = f1->second.find( toID );
	LOG_IF( f2 == f1->second.end(), Debug, error ) << "There is no known conversion from " << util::getTypeMap()[fromID] << " to " << util::getTypeMap()[toID];
	return f2->second;
}

const ValueArray::Converter & ValueArray::getConverterTo(unsigned short ID) const
{
	return getConverterFromTo(getTypeID(),ID);
}

std::size_t ValueArray::getLength() const
{
	return m_length;
}

bool ValueArray::isFloat() const
{
	return std::visit(
		[](auto ptr){
			return std::is_floating_point_v<typename decltype(ptr)::element_type>;
			},static_cast<const ArrayTypes&>(*this)
			);
}

bool ValueArray::isInteger() const
{
	return std::visit(
		[](auto ptr){
			return std::is_integral_v<typename decltype(ptr)::element_type>;
			},static_cast<const ArrayTypes&>(*this)
		);
}
std::ostream &operator<<(std::ostream &os, const ValueArray &array)
{
	assert(array.isValid());
	if ( array.isValid() ){
		os << "#" << array.getLength();
		if(array.getLength()) {//@todo use list2stream
			auto i = array.begin();
			os << isis::util::Value(*i).toString(false);
			for (++i; i < array.end(); i++)
				os << "|" << isis::util::Value(*i).toString(false);
		}
	}
	return os;
}

std::ostream &operator<<(std::ostream &os, const scaling_pair &pair)
{
	return os << std::make_pair(pair.scale,pair.offset);
}
}