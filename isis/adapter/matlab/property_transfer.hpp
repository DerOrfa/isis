//
// Created by Enrico Reimer on 18.08.20.
//

#pragma once

#include "isis/core/propmap.hpp"
#include "MatlabDataArray.hpp"

namespace isis::mat{
namespace _internal{

using namespace matlab::data;

template<typename T>  Array processArith(const util::PropertyValue &p,ArrayFactory &f){
    TypedArray<T> ret = f.createArray<T>({p.size(), 1});
    std::transform(p.begin(), p.end(), ret.begin(), [](const util::Value &v){return v.as<T>();});
    return ret;
}
template<typename T>  Array processStruct(const util::PropertyValue &p,ArrayFactory &f,std::complex<T>){
    return processArith<std::complex<T>>(p,f);//that actually works directly
}
template<typename T, std::size_t S>  Array processStruct(const util::PropertyValue &p,ArrayFactory &f,std::array<T,S>){
    auto ret = f.createArray<T>({p.size(), S});
    for(size_t j=0;j<p.size();j++) {
        const auto arr = p[j].as<std::array<T,S>>();
        for(size_t i=0;i<S;i++)
            ret[j][i] = arr[i];
    }
    return ret;
}
template<typename T>  Array processStruct(const util::PropertyValue &p,ArrayFactory &f,util::color<T>){
    auto ret = f.createArray<T>({p.size(), 3});
    for(size_t j=0;j<p.size();j++) {
        const auto col = p[j].as<util::color<T>>();
        ret[j][0] = col.r;
        ret[j][1] = col.g;
        ret[j][3] = col.b;
    }
    return ret;
}
template<typename T>  Array processStruct(const util::PropertyValue &p,ArrayFactory &f,std::list<T>) {
    auto ret = f.createCellArray({p.size(), 1});
    for(size_t j=0;j<p.size();j++) {
        const auto list = p[j].as<std::list<T>>();
        const auto vector = std::vector(list.begin(),list.end());//createArray needs random access iterators
        ret[j]=f.createArray({1,list.size()},vector.begin(),vector.end());
    }
    return ret;
}

template<typename T>  Array propToArray(const util::PropertyValue &p,ArrayFactory &f)
{
    if constexpr (std::is_arithmetic_v<T>)
        return processArith<T>(p,f);
    else
        return processStruct(p,f,T());
}
template<> Array propToArray<util::date>(const util::PropertyValue &p,ArrayFactory &f);
template<> Array propToArray<util::timestamp>(const util::PropertyValue &p,ArrayFactory &f);
template<> Array propToArray<util::duration>(const util::PropertyValue &p,ArrayFactory &f);
template<> Array propToArray<util::Selection>(const util::PropertyValue &p,ArrayFactory &f);
template<> Array propToArray<std::string>(const util::PropertyValue &p,ArrayFactory &f);

template<int ID = 0> std::vector<std::function<Array(const util::PropertyValue &p)>>
makePropTransfers(ArrayFactory &f)
{
	auto ret=std::move(makePropTransfers<ID + 1>(f));
	ret[ID]=[&f](const util::PropertyValue &p){return propToArray<util::Value::TypeByIndex<ID>>(p,f);};
	return ret;
}
//terminator, creates the container
template<> std::vector<std::function<Array(const util::PropertyValue &p)>>
makePropTransfers<util::Value::NumOfTypes>(ArrayFactory &);
}

matlab::data::StructArray branchToStruct(const util::PropertyMap &branch);

matlab::data::Array propertyToArray(const util::PropertyValue &p);

}