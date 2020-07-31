#ifndef LOG_MOUDLES_HPP_INCLUDED
#define LOG_MOUDLES_HPP_INCLUDED

namespace isis
{
struct CoreLog   {static constexpr char name[]="Core";      static constexpr bool use = _ENABLE_LOG;};
struct CoreDebug {static constexpr char name[]="CoreDebug"; static constexpr bool use = _ENABLE_DEBUG;};

struct ImageIoLog   {static constexpr char name[]="ImageIO";      static constexpr bool use = _ENABLE_LOG;};
struct ImageIoDebug {static constexpr char name[]="ImageIODebug"; static constexpr bool use = _ENABLE_DEBUG;};

struct DataLog   {static constexpr char name[]="Data";      static constexpr bool use = _ENABLE_LOG;};
struct DataDebug {static constexpr char name[]="DataDebug"; static constexpr bool use = _ENABLE_DEBUG;};

struct FilterLog   {static constexpr char name[]="Filter";      static constexpr bool use = _ENABLE_LOG;};
struct FilterDebug {static constexpr char name[]="FilterDebug"; static constexpr bool use = _ENABLE_DEBUG;};

struct MathLog   {static constexpr char name[]="Math";      static constexpr bool use = _ENABLE_LOG;};
struct MathDebug {static constexpr char name[]="MathDebug"; static constexpr bool use = _ENABLE_DEBUG;};
}

#endif //LOG_MOUDLES_HPP_INCLUDED
