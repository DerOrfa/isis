//
// Created by enrico on 11.07.21.
//

#pragma once

#include "pybind11/buffer_info.h"
#include "pybind11/pytypes.h"
#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"
#include "../../core/image.hpp"

namespace py = pybind11;

namespace isis::python{
struct Runtime   {static constexpr char name[]="Python";      static constexpr bool use = _ENABLE_LOG;};
struct Debug {static constexpr char name[]="PythonDebug"; static constexpr bool use = _ENABLE_DEBUG;};

template<typename HANDLE>
void enableLog(LogLevel level)
{
	ENABLE_LOG(Runtime, HANDLE, level);
	ENABLE_LOG(Debug, HANDLE, level);
}

class LoggerProxy: public util::MessageHandlerBase{
public:
	std::map<LogLevel,py::object> severity_map;
	py::object log;
	LoggerProxy(LogLevel level);
	void commit(const util::Message &msg) override;
};

py::array make_array(data::Chunk &ch);
py::array make_array(data::Image &img);

void setup_logging(LogLevel level=verbose_info);
//must be called before module is unloaded
//otherwise the some py::objects inside LoggerProxy will be left dangling
void free_logging(LogLevel level=notice);

std::pair<std::list<isis::data::Image>,std::list<std::string>>
load(std::string path,util::slist formatstack={},util::slist dialects={});

std::pair<std::list<isis::data::Image>,util::slist>
load_list(util::slist paths,util::slist formatstack={},util::slist dialects={});

std::variant<py::none, util::ValueTypes,std::list<util::ValueTypes>> property2python(util::PropertyValue val);
}