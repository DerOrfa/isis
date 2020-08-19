//
// Created by Enrico Reimer on 18.08.20.
//

#pragma once

#include "isis/core/propmap.hpp"
#include "MatlabDataArray.hpp"

namespace isis::mat{
namespace _internal{

using namespace matlab::data;

template<typename T> struct propToArrayOp
{
	ArrayFactory &f;
	Array operator()(const util::PropertyValue &p)
	{
		TypedArray<T> ret = f.createArray<T>({p.size(), 1});
		std::transform(p.begin(), p.end(), ret.begin(), [](const util::Value &v){return v.as<T>();});
		return ret;
	}
};
template<> Array propToArrayOp<util::date>::operator()(const util::PropertyValue &p);
template<> Array propToArrayOp<util::timestamp>::operator()(const util::PropertyValue &p);
template<> Array propToArrayOp<util::duration>::operator()(const util::PropertyValue &p);
template<> Array propToArrayOp<util::Selection>::operator()(const util::PropertyValue &p);
template<> Array propToArrayOp<std::string>::operator()(const util::PropertyValue &p);

template<typename T, int S> struct propToArrayOp<std::array<T,S>>
{
	ArrayFactory &f;
	Array operator()(const util::PropertyValue &p)
	{
		auto ret = f.createArray<T>({p.size(), S});
		for(size_t j=0;j<p.size();j++) {
			const auto arr = p[j].as<std::array<T,S>>();
			for(size_t i=0;i<S;i++)
				ret[j][i] = arr[i];
		}
		return ret;
	}
};
template<typename T> struct propToArrayOp<util::color<T>>
{
	ArrayFactory &f;
	Array operator()(const util::PropertyValue &p)
	{
		auto ret = f.createArray<T>({p.size(), 3});
		for(size_t j=0;j<p.size();j++) {
			const auto col = p[j].as<util::color<T>>();
			ret[j][0] = col.r;
			ret[j][1] = col.g;
			ret[j][3] = col.b;
		}
		return ret;
	}
};
template<typename T> struct propToArrayOp<std::list<T>>
{
	ArrayFactory &f;
	Array operator()(const util::PropertyValue &p)
	{
		auto ret = f.createCellArray({p.size(), 1});
		for(size_t j=0;j<p.size();j++) {
			const auto list = p[j].as<std::list<T>>();
			const auto vector = std::vector(list.begin(),list.end());//createArray needs random access iterators
			ret[j]=f.createArray({1,list.size()},vector.begin(),vector.end());
		}
		return ret;
	}
};

template<int ID = 0> std::vector<std::function<Array(const util::PropertyValue &p)>>
makePropTransfers(ArrayFactory &f)
{
	auto ret=std::move(makePropTransfers<ID + 1>(f));
	static propToArrayOp<util::Value::TypeByIndex<ID>> trans{f};
	ret[ID]=[](const util::PropertyValue &p){return trans(p);};
	return ret;
}
//terminator, creates the container
template<> std::vector<std::function<Array(const util::PropertyValue &p)>>
makePropTransfers<util::Value::NumOfTypes>(ArrayFactory &);
}

matlab::data::StructArray branchToStruct(const util::PropertyMap &branch);

matlab::data::Array propertyToArray(const util::PropertyValue &p);

}