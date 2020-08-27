#pragma once

#include <map>
#include <typeinfo>
#include <climits>
#include <cassert>
#include <typeindex>
#include <memory>

namespace isis::util
{

/**
 * Static class to handle singletons of a given type and priority.
 *
 * The special issues for these Singletons are: \n
 * 1) it's a template class - can be used for every type \n
 * 2) Singletons are deleted in ascending order of int values (0 first, INT_MAX last)
 *
 * \code
 * Singletons::get < MyClass, INT_MAX - 1 >()
 * \endcode
 * This generates a Singleton of MyClass with highest priority.
 * \note This is (currently) not thread save.
 */
class Singletons
{
	// a map typeid => unique_ptr<void> with dedicated delete functions, so we can hide the actual type from unique_ptr
	// in order to keep the pointer (and in extend the registry) type free
	using registry = std::map<std::type_index,std::unique_ptr<void,std::function<void(void const*)>>>;

	Singletons()=default;
	virtual ~Singletons();
	static std::map<int, Singletons::registry> &getRegistryStack();
public:
	/**
	 * The first call creates a singleton of type T with the priority PRIO (ascending order),
	 * all repetetive calls return this object.
	 * \return a reference to the same object of type T.
	 */
	template<class T, int PRIO = INT_MAX-1, typename... ARGS> static T &get(ARGS&&... args) {
		auto &registry=getRegistryStack()[PRIO];
		auto singleton_it = registry.find(typeid(T));
		if ( singleton_it==registry.end()) {
			// create singleton in unique_ptr where only the deleter knows the type
			registry::mapped_type ptr(
				new T(&args...),
				[](void const *p){delete(static_cast<T const*>(p));}
			);
			singleton_it=registry.emplace(typeid(T),std::move(ptr)).first;
		}
		assert(singleton_it!=registry.end());
		return *(static_cast<T*>(singleton_it->second.get()));
	}
};

}
