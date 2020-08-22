//
// Created by Enrico Reimer on 21.08.20.
//

#pragma once

#include "isis/core/image.hpp"
#include "MatlabDataArray.hpp"
#include <algorithm>

using namespace matlab::data;
using namespace isis;

namespace isis::mat
{
namespace _internal{
// map from isis type to matlab buffer type with proper size
template<typename T> struct makeBufferOp{
	buffer_ptr_t<T> createBuffer(size_t elements, ArrayFactory &f)const{
		return f.createBuffer<T>(elements);
	}
};
template<typename T,int S> struct makeBufferOp<std::array<T,S>>
{
	buffer_ptr_t<T> createBuffer(size_t elements, ArrayFactory &f)const{
		return f.createBuffer<T>(elements*S);
	}
};
template<typename T> struct makeBufferOp<util::color<T>>
{
	buffer_ptr_t<T> createBuffer(size_t elements, ArrayFactory &f)const{
		return f.createBuffer<T>(elements*3);
	}
};

}
template<typename SOURCE_TYPE,typename BUFFER_TYPE>
Array makeArray(buffer_ptr_t<BUFFER_TYPE> &&buffer,util::vector4<size_t> size, ArrayFactory &f){
	constexpr size_t size_fac=sizeof(SOURCE_TYPE)/sizeof(BUFFER_TYPE);
	//emulate squeeze function (and reverse ordering)
	std::vector<size_t> mat_size(5);
	auto last=std::copy_if(size.rbegin(),size.rend(),mat_size.begin(),[](size_t s){return s>1;});
	if(size_fac>1)
		*(last++)=size_fac;
	mat_size.resize(std::distance(mat_size.begin(),last));
	return f.createArrayFromBuffer(mat_size, std::move(buffer),MemoryLayout::ROW_MAJOR);
}
Array chunkToArray(const data::Chunk &chk);
Array imageToArray(const data::Image &img);
}