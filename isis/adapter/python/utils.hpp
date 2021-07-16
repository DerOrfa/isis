//
// Created by enrico on 11.07.21.
//

#pragma once

#include <pybind11/buffer_info.h>
#include <pybind11/pytypes.h>
#include "../../core/image.hpp"

namespace py = pybind11;



namespace isis::python{
py::buffer_info make_buffer(const data::Chunk &ch);
std::pair<std::list<isis::data::Image>,std::list<std::string>>
load(std::string path,util::slist formatstack={},util::slist dialects={});

std::pair<std::list<isis::data::Image>,util::slist>
load_list(util::slist paths,util::slist formatstack={},util::slist dialects={});

std::variant<py::none, util::ValueTypes,std::list<util::ValueTypes>> property2python(util::PropertyValue val);
}