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
	auto name_gen = [](const util::PropertyMap::PropPath  &p){
		static const std::regex matlab_fieldname("[^_[:alnum:]]",std::regex_constants::ECMAScript|std::regex_constants::optimize);
		return std::regex_replace(p.toString(), matlab_fieldname, "_");
	};
	std::transform(props.begin(),props.end(),names.begin(),name_gen);
	std::transform(branches.begin(),branches.end(),names.begin()+props.size(),name_gen);

	//make names unique (todo might cause data loss)
	std::sort(names.begin(),names.end());
	names.resize(std::distance(names.begin(),std::unique(names.begin(),names.end())));//resize to new end

	auto ret=f.createStructArray({1,1},names);

	for(const auto &name:props){
		ret[0][name_gen(name)] = propertyToArray(*branch.queryProperty(name));
	}
	for(const auto &name:branches){
		ret[0][name_gen(name)] = branchToStruct(*branch.queryBranch(name));
	}
	return std::move(ret);
}

Array mat::propertyToArray(const util::PropertyValue &p)
{
	static ArrayFactory f;
	static auto transfers=_internal::makePropTransfers(f);
	return transfers[p.getTypeID()](p);
}
