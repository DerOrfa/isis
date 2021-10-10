#pragma once

#include <map>
#include <typeinfo>
#include <climits>
#include <cassert>
#include <typeindex>
#include <memory>
#include <functional>

namespace isis::util
{

/**
 * Static class to handle singletons of a given type and priority.
 *
 * The special issues for these Singletons are: \n
 * 1) It is using a static template function that can by used for any object class \n
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
	// unique_ptr<void> with a dedicated delete functions, so we can hide the actual type from unique_ptr
	// in order to keep the pointer (and in extend the registry) type free
	using single_ptr = std::unique_ptr<void,std::function<void(void const*)>>;
	// and an attached priority
	using singleton = std::pair<int,single_ptr>;
	using registry = std::map<std::type_index,singleton>;

	Singletons()=default;
	~Singletons();
	static registry &getRegistry();
public:
	/**
	 * The first call creates a singleton of type T with the priority PRIO (ascending order),
	 * all repetitive calls return this object.
	 * PRIO is ignored on repeated calls
	 * \return a reference to the same object of type T.
	 */
	template<class T, int PRIO = INT_MAX-1, typename... ARGS> static T &get(ARGS&&... args) {
		singleton &sngl= getRegistry()[typeid(T)];
		if ( !sngl.second ) {
			// create singleton in unique_ptr where only the deleter knows the type
			sngl = {PRIO,single_ptr (
				new T(&args...),
				[](void const *p){delete(static_cast<T const*>(p));}
			)};
		}
		assert(sngl.second);
		return *(static_cast<T*>(sngl.second.get()));
	}
};

}
