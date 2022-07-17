//
// Created by reimer on 7/13/22.
//

#pragma once
#ifdef __clang__
	#include <experimental/coroutine>
	using std::experimental::coroutine_handle;
	using std::experimental::suspend_never;
	using std::experimental::suspend_always;
#else
	#include <coroutine>
	using std::coroutine_handle;
	using std::suspend_never;
	using std::suspend_always;
#endif
#include "bytearray.hpp"
#include "common.hpp"
#include "types.hpp"

namespace isis::io
{

template<typename T> struct Generator {
	struct promise_type;
	using handle_type = coroutine_handle<promise_type>;

	struct promise_type { // required
		T value_;
		std::exception_ptr exception_;

		Generator get_return_object() {return Generator(handle_type::from_promise(*this));}
		suspend_never initial_suspend() {return {};}
		suspend_always final_suspend() noexcept {return {};}
		void unhandled_exception() { exception_ = std::current_exception(); } // saving exception

		template<class From> suspend_always yield_value(From &&from) {
			value_ = std::move(from); // caching the result in promise
			return {};
		}
		void return_void() {}
	};

	handle_type h_;

	Generator(handle_type h) : h_(h) {}
	~Generator() { h_.destroy(); }
	explicit operator bool(){
		return !h_.done();
	}
	T operator()() {
		T ret= std::move(h_.promise().value_);
		h_.resume();
		return ret;
	}
};


class IoProtocol
{
public:
	typedef std::variant<std::filebuf, data::ByteArray> io_object;
	typedef std::tuple<std::string,io_object> load_result;
	virtual Generator<load_result> load(std::string path) = 0;
	virtual util::slist prefixes() = 0;
	virtual util::slist suffixes() = 0;

};

class DirectoryProtocol : public IoProtocol{
	static const size_t mapping_size = 1024*1024*10;
public:
	util::slist prefixes() override;
	util::slist suffixes() override;
	Generator<load_result> load(std::string path) override;
};
}
