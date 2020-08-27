#include "singletons.hpp"

namespace isis::util
{

std::map<int, Singletons::registry> & Singletons::getRegistryStack()
{
	static std::map<int, registry > registry_stack;
	return registry_stack;
}
Singletons::~Singletons()
{
	auto &stack=getRegistryStack();
	while ( !stack.empty() ) {//remove all registries beginning at the lowest priority
		stack.erase( stack.begin() );
	}
}

}
