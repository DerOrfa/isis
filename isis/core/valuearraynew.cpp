#include "valuearraynew.hpp"
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
    const std::shared_ptr<const uint8_t> b_ptr = std::visit(
                [](const auto &p){
        return std::static_pointer_cast<const uint8_t>(p);
    },*this);
    if( offset ) {
        _internal::DelProxy proxy( *this );
        return std::shared_ptr<const void>( b_ptr.get()+offset, proxy );
    } else
        return std::static_pointer_cast<const void>( b_ptr );
}

std::string isis::data::ValueArrayNew::typeName() const{
    return std::visit(_internal::arrayname_visitor(),*this);
}

std::shared_ptr<void> isis::data::ValueArrayNew::getRawAddress(size_t offset) { // use the const version and cast away the const
    return std::const_pointer_cast<void>( this->getRawAddress( offset ) );
}

isis::data::ValueArrayNew::value_iterator isis::data::ValueArrayNew::beginGeneric() {
	const size_t bytesize=bytesPerElem();
	auto visitor = [bytesize](auto ptr){
		return value_iterator( 
			( uint8_t * )ptr.get(), ( uint8_t * )ptr.get(), bytesize, 
			getValueFrom<decltype(ptr)::element_type>, setValueInto<decltype(ptr)::element_type>
		);
	};
	return std::visit(visitor,*this);
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
    std::string ret;

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
			ret[i]=ValueArrayNew( ptr.get() + i * size, size, proxy ) );

		if ( lastSize )
			ret.back()= ValueArrayNew( ptr.get() + fullSplices * size, lastSize, proxy );
		
		return ret;
	};

	return std::visit(generator,*this);;
}

isis::data::scaling_pair isis::data::ValueArrayNew::getScalingTo(unsigned short typeID, isis::data::autoscaleOption scaleopt) const {
    if( typeID == index() && scaleopt == autoscale ) { // if id is the same and autoscale is requested
        static const util::ValueNew one( uint8_t(1) );
        static const util::ValueNew zero( uint8_t(0) );
        return std::pair<util::ValueNew, util::ValueNew>( one, zero ); // the result is always 1/0
    } else { // get min/max and compute the scaling
        std::pair<util::ValueNew, util::ValueNew> minmax = getMinMax();
        return ValueArrayNew::getScalingTo( typeID, minmax, scaleopt );
    }
}

std::size_t isis::data::ValueArrayNew::bytesPerElem() const{
	return std::visit([](auto ptr){return sizeof(decltype(ptr)::element_type);},*this);
}

void isis::data::ValueArrayNew::endianSwap() {
	const size_t len=m_length;
	if(bytesPerElem()>1){
		std::visit([len](auto ptr){
			data::endianSwapArray( ptr.get(), ptr.get()+len, ptr.get() );
		});
	}

}

std::pair<isis::util::ValueNew, isis::util::ValueNew> isis::data::ValueArrayNew::getMinMax() const{
    if ( getLength() == 0 ) {
        LOG( Debug, error ) << "Skipping computation of min/max on an empty ValueArray";
        return std::pair<util::ValueNew, util::ValueNew>();
    } else {
		_internal::getMinMaxVisitor visitor;
		std::visit(visitor,*this);
        return visitor.minmax;
    }
}


