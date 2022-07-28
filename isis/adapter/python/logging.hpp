#pragma once

#include "pybind11/pybind11.h"
#include "../../core/log.hpp"
#include "../../core/istring.hpp"
#include "../../core/selection.hpp"

namespace py = pybind11;

namespace isis::python
{
namespace _internal{
	template<typename... ARGS>  py::object callMember(py::object object,const char *fname,ARGS... param){
		return py::object(object.attr(fname))(param...);
	}
}
struct Runtime{static constexpr char name[] = "Python";	static constexpr bool use = _ENABLE_LOG;};
struct Debug{static constexpr char name[] = "PythonDebug";	static constexpr bool use = _ENABLE_DEBUG;};

extern std::map<util::istring,std::function<void(LogLevel)>> setters;

template<const char *NAME>
class LoggerProxy : public util::MessageHandlerBase
{
private:
	py::object log=py::none();
	void init(){
		const auto handlers = logging.attr("root").attr("handlers");
		if(handlers.begin() == handlers.end()){ //logging not set up yet
			_internal::callMember(logging,"basicConfig");
		}

		auto mylogger = _internal::callMember(logging,"getLogger",std::string("isis.")+NAME);
		_internal::callMember(mylogger,"setLevel",severity_map[m_level]);
		log = mylogger.attr("log");
	}
public:
	std::map<LogLevel, py::object> severity_map;
	py::module_ logging;
	LoggerProxy(LogLevel level) : util::MessageHandlerBase(level)
	{
		logging = py::module_::import("logging");

		severity_map[error] = logging.attr("ERROR");
		severity_map[warning] = logging.attr("WARNING");
		severity_map[notice] = logging.attr("INFO");
		severity_map[info] = logging.attr("INFO");
		severity_map[verbose_info] = logging.attr("DEBUG");
	}
	void commit(const util::Message &msg) override
	{
		if(log.is_none())
			init();
		log(severity_map[msg.m_level], msg.merge(""));
	}

};

void setLogging(const util::Selection &lvl,util::istring module="all");
void setup_logging(LogLevel level = notice);
//must be called before module is unloaded
//otherwise the some py::objects inside LoggerProxy will be left dangling
void free_logging(LogLevel level = notice);
}
