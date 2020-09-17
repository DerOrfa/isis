//
// Created by Enrico Reimer on 21.08.20.
//

#pragma once

#include "isis/core/image.hpp"
#include "MatlabDataArray.hpp"
#include "common.hpp"
#include <algorithm>

using namespace matlab::data;
using namespace isis;

namespace isis::mat
{
namespace _internal{
template<typename T,std::size_t S> buffer_ptr_t<T> makeStructBuffer(size_t elements, ArrayFactory &f, const std::array<T,S>&){
    return f.createBuffer<T>(elements*S);
}
template<typename T> buffer_ptr_t<T> makeStructBuffer(size_t elements, ArrayFactory &f, const util::color<T>&){
    return f.createBuffer<T>(elements*3);
}
template<typename T> buffer_ptr_t<std::complex<T>> makeStructBuffer(size_t elements, ArrayFactory &f, const std::complex<T>&){
    return f.createBuffer<std::complex<T>>(elements*3);//this is actually available in matlab
}

// map from isis type to matlab buffer type with proper size
template<typename T> auto makeBuffer(size_t elements, ArrayFactory &f){
    if constexpr (std::is_arithmetic_v<T>) {
        LOG(MathDebug,info) << "Creating native " << util::MSubject(util::typeName<T>()) << "-buffer for " << elements << " elements";
        return f.createBuffer<T>(elements);
    } else {
        LOG(MathDebug,info) << "Creating complex " << util::MSubject(util::typeName<T>()) << "-buffer for " << elements << " elements";
        return makeStructBuffer(elements, f, T());
    }
}

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
    LOG(MatlabDebug,info)
        << "Making matlab array of size " << util::listToString(mat_size.begin(),mat_size.end())
        << " from " << util::typeName<BUFFER_TYPE>() << " buffer";

    return f.createArrayFromBuffer(mat_size, std::move(buffer),MemoryLayout::ROW_MAJOR);
}
Array chunkToArray(const data::Chunk &chk);
Array imageToArray(const data::Image &img);
}