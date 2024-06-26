#pragma once

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#define API_EXCLUDE_BEGIN _Pragma("GCC visibility push(hidden)")
#define API_EXCLUDE_END   _Pragma("GCC visibility pop")
#else
#define API_EXCLUDE_BEGIN
#define API_EXCLUDE_END
#endif

#include <cstddef>
#include <stdint.h>
#include <string>

using std::size_t;
using namespace std::literals::string_literals;

