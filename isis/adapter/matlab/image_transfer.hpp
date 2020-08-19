//
// Created by Enrico Reimer on 21.08.20.
//

#pragma once

#include "isis/core/image.hpp"
#include "MatlabDataArray.hpp"

using namespace matlab::data;
using namespace isis;

namespace isis::mat
{
namespace _internal{
template<typename T> struct ptrTransferOp
{
	TypedArray<T> operator()(std::shared_ptr<T> ptr, util::vector4<size_t> size, ArrayFactory &f)const
	{
		const T *begin = ptr.get(), *end = ptr.get() + util::product(size);
		return f.createArray<T>({size[0], size[1], size[2], size[3]}, begin, end);
	}
};

template<typename T,int S> struct ptrTransferOp<std::array<T,S>>
{
	TypedArray<T> operator()(std::shared_ptr<std::array<T,S>> ptr, util::vector4<size_t> size, ArrayFactory &f)const
	{
		auto ret=f.createCellArray({size[0], size[1], size[2], size[3]});
		const std::array<T,S> *s_it=ptr.get();
		for(auto r_it=ret.begin();r_it!=ret.end();++r_it,++s_it){
			*r_it = f.createArray<T>({S,1},std::begin(*s_it),std::end(*s_it));
		}
		return ret;
	}
};
template<typename T> struct ptrTransferOp<util::color<T>>
{
	TypedArray<T> operator()(std::shared_ptr<util::color<T>> ptr, util::vector4<size_t> size, ArrayFactory &f)const
	{
		auto ret=f.createCellArray({size[0], size[1], size[2], size[3]});
		const util::color<T> *s_it=ptr.get();
		for(auto r_it=ret.begin();r_it!=ret.end();++r_it,++s_it){
			*r_it = f.createArray<T>({3,1},{s_it->r,s_it->g,s_it->b});
		}
		return ret;
	}
};
}
Array chunkToArray(const data::Chunk &chk);
}