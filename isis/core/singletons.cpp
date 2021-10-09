#include "singletons.hpp"

namespace isis::util
{

Singletons::registry & Singletons::getRegistry()
{
	static registry static_reg;
	return static_reg;
}
Singletons::~Singletons()
{
	// transfer all singletons (aka unique pointers) into a priority sorted list and delete them accordingly
	auto &reg= getRegistry();
	std::map<int,single_ptr> delete_stack;
	for(auto &v:reg){//the mapped value of the registry reg is a PRIO/single_ptr pair
		delete_stack.insert(std::move(v.second)); //exactly what insert expects (we use move here as unique_ptr can not be copied)
	}//reg now is full of null-pointers
	//remove all registries beginning at the lowest priority
	while ( !delete_stack.empty() ) {
		delete_stack.erase(delete_stack.begin() );
	}
}

}
