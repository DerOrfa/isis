//
// Created by enrico on 11.07.21.
//

#pragma once

#include <pybind11/buffer_info.h>
#include "../../core/image.hpp"

namespace py = pybind11;



namespace isis::python{
py::buffer_info make_buffer(const data::Chunk &ch);
std::pair<std::list<isis::data::Image>,std::list<std::string>>
load(std::string path,util::slist formatstack={},util::slist dialects={});

std::pair<std::list<isis::data::Image>,std::list<std::string>>
load_list(util::slist paths,util::slist formatstack={},util::slist dialects={});
}