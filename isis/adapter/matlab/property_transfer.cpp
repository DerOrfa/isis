//
// Created by Enrico Reimer on 18.08.20.
//

#include "property_transfer.hpp"

using namespace isis;
using namespace matlab::data;

namespace isis::mat::_internal{
template<> Array propToArrayOp<util::date>::operator()(const util::PropertyValue &p){
	return f.createScalar(p.toString());
}
template<> Array propToArrayOp<util::timestamp>::operator()(const util::PropertyValue &p){
	return f.createScalar(p.toString());
}
template<> Array propToArrayOp<util::duration>::operator()(const util::PropertyValue &p){
	return f.createScalar(p.toString());
}
template<> Array propToArrayOp<util::Selection>::operator()(const util::PropertyValue &p){
	return f.createScalar(p.toString());
}
template<> Array propToArrayOp<std::string>::operator()(const util::PropertyValue &p){
	return f.createScalar(p.toString());
}
//terminator, creates the container
template<> std::vector<std::function<Array(const util::PropertyValue &p)>>
makePropTransfers<util::Value::NumOfTypes>(ArrayFactory &)
{
	return std::vector<std::function<Array(const util::PropertyValue &p)>>(util::Value::NumOfTypes);
}
}

StructArray mat::branchToStruct(const util::PropertyMap &branch)
{
	static ArrayFactory f;
	const auto props = branch.localProps();
	const auto branches = branch.localBranches();
	std::vector<std::string> names(props.size()+branches.size());
	std::transform(props.begin(),props.end(),names.begin(),[](const util::PropertyMap::PropPath  &p){return p.toString();});
	std::transform(branches.begin(),branches.end(),names.begin()+props.size(),[](const util::PropertyMap::PropPath  &p){return p.toString();});

	auto ret=f.createStructArray({1,1},names);

	for(const auto &name:props){
		ret[0][name.toString()] = propertyToArray(*branch.queryProperty(name));
	}
	return std::move(ret);
}

Array mat::propertyToArray(const util::PropertyValue &p)
{
	static ArrayFactory f;
	static auto transfers=_internal::makePropTransfers(f);
	return transfers[p.getTypeID()](p);
}
