#include "valuearray.hpp"
#include "valuearray_converter.hpp"
#include "endianess.hpp"

isis::data::_internal::DelProxy::DelProxy(const isis::data::ValueArrayNew& master)
{
	LOG( Debug, verbose_info ) << "Creating DelProxy for " << master.typeName() << " at " << this->get();
}
void isis::data::_internal::DelProxy::operator()(const void* at)
{
	LOG( Debug, verbose_info )
	        << "Deletion for " << this->get() << " called from proxy at offset "
	        << static_cast<const uint8_t *>( at ) - static_cast<const uint8_t *>( this->get() )
	        << ", current use_count: " << this->use_count();
	this->reset();//actually not needed, but we keep it here to keep obfuscation low
}

std::shared_ptr<const void> isis::data::ValueArrayNew::getRawAddress(size_t offset) const{
	const std::shared_ptr<const void> b_ptr = std::visit(
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

std::shared_ptr<void> isis::data::ValueArrayNew::getRawAddress(size_t offset) { // use the const version and cast away the const
	const std::shared_ptr<const void> ptr=const_cast<const ValueArrayNew*>(this)->getRawAddress( offset );
	return std::const_pointer_cast<void>( ptr );
}

std::string isis::data::ValueArrayNew::typeName() const{
	return std::visit(_internal::arrayname_visitor(),static_cast<const ArrayTypes&>(*this));
}

isis::data::ValueArrayNew::value_iterator isis::data::ValueArrayNew::beginGeneric() {
	const size_t bytesize=bytesPerElem();
	auto visitor = [bytesize](auto ptr)->value_iterator{
		typedef typename decltype(ptr)::element_type element_type;
		return value_iterator(
		    ( uint8_t * )ptr.get(), ( uint8_t * )ptr.get(), bytesize,
		    getValueFrom<element_type>, setValueInto<element_type>
		);
	};
	return std::visit(visitor,static_cast<ArrayTypes&>(*this));
}
isis::data::ValueArrayNew::const_value_iterator isis::data::ValueArrayNew::beginGeneric() const {
	return const_cast<ValueArrayNew*>(this)->beginGeneric();
}

isis::data::ValueArrayNew::value_iterator isis::data::ValueArrayNew::endGeneric() {
	return beginGeneric()+m_length;
}

isis::data::ValueArrayNew::const_value_iterator isis::data::ValueArrayNew::endGeneric() const {
	return beginGeneric()+m_length;
}

std::string isis::data::ValueArrayNew::toString(bool labeled) const {
	std::string ret;//@todo listToString

	if ( m_length ) {
		for ( auto i = beginGeneric(); i < endGeneric() - 1; i++ )
			ret += util::ValueNew( *i ).toString( false ) + "|";


		ret += util::ValueNew( *( endGeneric() - 1 ) ).toString( labeled );
	}

	return std::to_string( m_length ) + "#" + ret;
}

std::vector<isis::data::ValueArrayNew> isis::data::ValueArrayNew::splice(size_t size) const{
	if ( size >= getLength() ) {
		LOG( Debug, warning )
		        << "splicing data of the size " << getLength() << " up into blocks of the size " << size << " is kind of useless ...";
	}

	const size_t fullSplices = getLength() / size;
	const size_t lastSize = getLength() % size;//rest of the division - size of the last splice
	const size_t splices = fullSplices + ( lastSize ? 1 : 0 );

	_internal::DelProxy proxy( *this );

	auto generator = [&](auto ptr){
		std::vector<ValueArrayNew> ret( splices );

		for ( size_t i = 0; i < fullSplices; i++ )
			ret[i]=ValueArrayNew( ptr.get() + i * size, size, proxy );

		if ( lastSize )
			ret.back()= ValueArrayNew( ptr.get() + fullSplices * size, lastSize, proxy );

		return ret;
	};

	return std::visit(generator,static_cast<const ArrayTypes&>(*this));
}

isis::data::scaling_pair isis::data::ValueArrayNew::getScalingTo(unsigned short typeID) const {
	return getScalingTo(typeID, getMinMax());
}
isis::data::scaling_pair isis::data::ValueArrayNew::getScalingTo( unsigned short typeID, const std::pair<util::ValueNew, util::ValueNew> &minmax )const
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

std::size_t isis::data::ValueArrayNew::bytesPerElem() const{
	return std::visit([](auto ptr){return sizeof(typename decltype(ptr)::element_type);},static_cast<const ArrayTypes&>(*this));
}

void isis::data::ValueArrayNew::endianSwap() {
	const size_t len=m_length;
	if(bytesPerElem()>1){
		std::visit([len](auto ptr){
			data::endianSwapArray( ptr.get(), ptr.get()+len, ptr.get() );
		},static_cast<const ArrayTypes&>(*this));
	}

}

std::pair<isis::util::ValueNew, isis::util::ValueNew> isis::data::ValueArrayNew::getMinMax() const{
	if ( getLength() == 0 ) {
		LOG( Debug, error ) << "Skipping computation of min/max on an empty ValueArray";
		return std::pair<util::ValueNew, util::ValueNew>();
	} else {
		_internal::getMinMaxVisitor visitor;
		std::visit(visitor,static_cast<const ArrayTypes&>(*this));
		return visitor.minmax;
	}
}

const isis::data::_internal::ValueArrayConverterMap & isis::data::ValueArrayNew::converters(){
	static const _internal::ValueArrayConverterMap map;
	return map;
}

isis::data::ValueArrayNew::ValueArrayNew():m_length(0)
{}

bool isis::data::ValueArrayNew::isValid() const{
	return index()!=std::variant_npos && std::visit([](auto ptr){return (bool)ptr;}, static_cast<const ArrayTypes&>(*this));
}

isis::data::ValueArrayNew isis::data::ValueArrayNew::copyByID(size_t ID, const scaling_pair &scaling) const
{
	const Converter &conv = getConverterTo( ID );

	if( conv ) {
		return conv->generate( *this, getScaling( scaling, ID ) );
	} else {
		LOG( Runtime, error ) << "I don't know any conversion from " << typeName() << " to " << util::getTypeMap( true )[ID];
		return ValueArrayNew(); // return an invalid array
	}
}
bool isis::data::ValueArrayNew::copyTo(isis::data::ValueArrayNew& dst, isis::data::scaling_pair scaling) const
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

void isis::data::ValueArrayNew::copyRange(std::size_t start, std::size_t end, isis::data::ValueArrayNew& dst, std::size_t dst_start) const
{
	assert( start <= end );
	const size_t len = end - start + 1;
	LOG_IF( ! this->getTypeID()==dst.getTypeID(), Debug, error )
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
std::size_t isis::data::ValueArrayNew::getTypeID() const{
	return std::visit(
	    [](auto ptr){return util::typeID<typename decltype(ptr)::element_type>();},
	    static_cast<const ArrayTypes&>(*this)
	);
}

isis::data::ValueArrayNew isis::data::ValueArrayNew::convertByID(unsigned short ID, scaling_pair scaling) const{
	scaling = getScaling(scaling, ID);
	
	assert(scaling.valid );
	if( !scaling.isRelevant() && getTypeID() == ID ) { // if type is the same and scaling is 1/0
		return *this; //cheap copy
	} else {
		return copyByID( ID, scaling ); // convert into new
	}
}

isis::data::ValueArrayNew isis::data::ValueArrayNew::createByID(unsigned short ID, std::size_t len)
{
	const _internal::ValueArrayConverterMap::const_iterator f1 = converters().find( ID );
	_internal::ValueArrayConverterMap::mapped_type::const_iterator f2;
	ValueArrayNew ret;
	// try to get a converter to convert the requested type into itself - they're there for all known types
	if( f1 != converters().end() && ( f2 = f1->second.find( ID ) ) != f1->second.end() ) {
		const ValueArrayConverterBase &conv = *( f2->second );
		ret=conv.create( len );
		LOG_IF(!ret.isValid(),Runtime,error) << "The created array is not valid, this is not going to end well..";
	} else {
		LOG( Debug, error ) << "There is no known creator for " << util::getTypeMap()[ID];
	}
	return ret;
}

std::size_t isis::data::ValueArrayNew::useCount() const
{
	return std::visit([](auto ptr){return ptr.use_count();},static_cast<const ArrayTypes&>(*this));
}

isis::data::ValueArrayNew isis::data::ValueArrayNew::cloneToNew(std::size_t length) const
{
	return createByID(getTypeID(),length);
}

std::size_t isis::data::ValueArrayNew::compare(std::size_t start, std::size_t end, const isis::data::ValueArrayNew& dst, std::size_t dst_start) const
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

	// lock the memory so we can mem-compare the elements (use uint8_t because some compilers do not like arith on void*)
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

const isis::data::ValueArrayNew::Converter & isis::data::ValueArrayNew::getConverterFromTo(unsigned short fromID, unsigned short toID)
{
	const _internal::ValueArrayConverterMap::const_iterator f1 = converters().find( fromID );
	LOG_IF( f1 == converters().end(), Debug, error ) << "There is no known conversion from " << util::getTypeMap()[fromID];
	const _internal::ValueArrayConverterMap::mapped_type::const_iterator f2 = f1->second.find( toID );
	LOG_IF( f2 == f1->second.end(), Debug, error ) << "There is no known conversion from " << util::getTypeMap()[fromID] << " to " << util::getTypeMap()[toID];
	return f2->second;
}

const isis::data::ValueArrayNew::Converter & isis::data::ValueArrayNew::getConverterTo(unsigned short ID) const
{
	return getConverterFromTo(getTypeID(),ID);
}

std::size_t isis::data::ValueArrayNew::getLength() const
{
	return m_length;
}

bool isis::data::ValueArrayNew::isFloat() const
{
	return std::visit(
	    [](auto ptr){
		    return std::is_floating_point_v<typename decltype(ptr)::element_type>;
	    },static_cast<const ArrayTypes&>(*this)
	);
}

bool isis::data::ValueArrayNew::isInteger() const
{
	return std::visit(
	    [](auto ptr){
		    return std::is_integral_v<typename decltype(ptr)::element_type>;
	    },static_cast<const ArrayTypes&>(*this)
	);
}

