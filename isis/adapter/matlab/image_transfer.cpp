//
// Created by Enrico Reimer on 21.08.20.
//

#include "image_transfer.hpp"
#include "isis/core/image.hpp"
#include <map>

namespace isis::mat{
namespace _internal{

template<int ID = 0> std::map<size_t,std::function<Array(const data::Image&)>> makeImageTransfers(ArrayFactory &f)
{
	using image_type=typename data::ValueArray::TypeByIndex<ID>::element_type;//this is NOT the isis typeID
	auto map=std::move(makeImageTransfers<ID+1>(f));
	map[util::typeID<image_type>()] = //this is
		[&f](const data::Image &img)->Array{
			const size_t elements=img.getVolume();
			auto buffer = makeBuffer<image_type>(elements,f);
			img.copyToMem(reinterpret_cast<image_type*>(buffer.get()),elements );
			return makeArray<image_type>(std::move(buffer),img.getSizeAsVector(),f);
		};
	return std::move(map);
}
//terminator
template<> std::map<size_t,std::function<Array(const data::Image&)>>
makeImageTransfers<std::variant_size_v<data::ArrayTypes>>(ArrayFactory &f){return {};}
}


Array chunkToArray(const data::Chunk &chk)
{
	static ArrayFactory f;
	const auto size=chk.getSizeAsVector();
	std::shared_ptr<int> ptr;

	return chk.visit([size](auto ptr)->Array{
		using element_type = typename decltype(ptr)::element_type;
		auto buffer = _internal::makeBuffer<element_type>(util::product(size),f);
		const size_t copy_bytes = util::product(size)*sizeof(element_type);
		memcpy(buffer.get(),ptr.get(),copy_bytes);
		return makeArray<element_type>( std::move(buffer),size,f);
	});
}
Array imageToArray(const data::Image &img)
{
	static ArrayFactory f;
	static const auto transfers =_internal::makeImageTransfers(f);
	auto found_transfer = transfers.find(img.getMajorTypeID());
	if(found_transfer!=transfers.end()){
		return found_transfer->second(img);
	} else {
		LOG(MathLog, error) << "No transfer function found for " << img.getMajorTypeName() << "-image";
		return Array();
	}
}
}