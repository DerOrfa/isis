//
// Created by reimer on 7/13/22.
//

#pragma once

#if __has_include ( <coroutine> )
#include <coroutine>
using std::coroutine_handle;
using std::suspend_never;
using std::suspend_always;
#elif __has_include ( <experimental/coroutine> )
#include <experimental/coroutine>
	using std::experimental::coroutine_handle;
	using std::experimental::suspend_never;
	using std::experimental::suspend_always;
#else
	static_assert(false,"this needs coroutine support");
#endif


#include "bytearray.hpp"
#include "chunk.hpp"
#include "common.hpp"
#include <fstream>
#include <future>
#include <any>

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
			value_ = std::forward<From>(from); // caching the result in promise
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
	template<class T, typename... ARGS> static std::any* init(const ARGS&... args) requires std::is_base_of_v<IoProtocol,T>
	{
		return new std::any(T(args...));
	}
	typedef std::variant<std::unique_ptr<std::streambuf>, data::ByteArray, data::Chunk> io_object;
	typedef std::tuple<std::string,std::future<io_object>> load_result;
};

io::Generator<IoProtocol::load_result>
protocol_load( const std::string &path, const std::list<util::istring>& formatstack = {}, const std::list<util::istring>& dialects = {}, util::slist* rejected=nullptr);
io::Generator<IoProtocol::load_result>
protocol_load(io::Generator<IoProtocol::load_result> &&outer_generator, const std::list<util::istring>& formatstack = {}, const std::list<util::istring>& dialects = {}, util::slist* rejected=nullptr);


class PrimaryIoProtocol: public IoProtocol
{
public:
	virtual Generator<load_result> load(std::string path, const std::list<util::istring> &dialects) = 0;
	virtual util::slist prefixes() = 0;
	virtual ~PrimaryIoProtocol() = default;
};
class SecondaryIoProtocol: public IoProtocol
{
public:
	virtual Generator<load_result> load(std::future<io_object> obj, const std::list<util::istring> &dialects) = 0;
	virtual util::slist suffixes() = 0;
	virtual ~SecondaryIoProtocol() = default;
};

}

extern "C" {
#ifdef WIN32
extern __declspec( dllexport ) void *init();
extern __declspec( dllexport ) const char *name();
#else
extern void *init();
extern const char *name();
#endif
}
