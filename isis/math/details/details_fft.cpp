#include "details_fft.hxx"


void isis::math::_internal::halfshift(isis::data::ValueArrayNew &src){
	//shift backwards
	assert(src.getLength()%2==0);
	const size_t shiftsize=src.getLength()/2*src.bytesPerElem();
	std::shared_ptr<uint8_t> begin=std::static_pointer_cast<uint8_t>(src.getRawAddress());

	std::shared_ptr<uint8_t> buffer((uint8_t*)malloc(shiftsize));
	memcpy(buffer.get(),         begin.get(),          shiftsize);
	memcpy(begin.get(),          begin.get()+shiftsize,shiftsize);
	memcpy(begin.get()+shiftsize,buffer.get(),         shiftsize);
}

void isis::math::_internal::halfshift(isis::data::Chunk &data){
	isis::data::ValueArrayNew &array=data;
	for(size_t rank=0;rank<data.getRelevantDims();rank++){
		std::array<size_t,4> dummy_index={0,0,0,0};
		dummy_index[rank]=1;
		//spliceAt into lines of dimsize elements
		size_t stride=data.getLinearIndex(dummy_index);
		std::vector< data::ValueArrayNew > lines= 
			(rank<data.getRelevantDims()-1) ? //do not call spliceAt for the top rank (full volume)
				array.splice(data.getDimSize(rank)*stride):
				std::vector<data::ValueArrayNew>(1,array);

		for(data::ValueArrayNew line:lines){
			halfshift(line);
		}
	}
}
