//
// Created by Enrico Reimer on 21.08.20.
//

#include "image_transfer.hpp"

Array isis::mat::chunkToArray(const data::Chunk &chk)
{
	static ArrayFactory f;
	const auto size=chk.getSizeAsVector();
	std::shared_ptr<int> ptr;

	return chk.visit([size](auto ptr)->Array{
		using element_type = typename decltype(ptr)::element_type;
		const _internal::ptrTransferOp<element_type> op;
		return op(ptr,size,f);
	});
}
