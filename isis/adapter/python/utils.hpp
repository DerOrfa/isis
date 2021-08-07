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

//mapping pybind format string back to isis type-ids
struct TypeMap : std::map<std::string,size_t>{
	TypeMap(){fill();}
	template<size_t ID=1> void fill(){
		if constexpr(ID<std::variant_size_v<util::ValueTypes>){
			typedef util::Value::TypeByIndex<ID> isis_type;
			typedef pybind11::detail::is_fmt_numeric<isis_type> py_type;
			if constexpr(py_type::value)
				emplace_hint(end(),pybind11::format_descriptor<isis_type>::format(),ID);
			fill<ID+1>();
		}
	}
};

py::array make_array(data::Chunk &ch);
py::array make_array(data::Image &img);

std::pair<std::list<isis::data::Image>,std::list<std::string>>
load(std::string path,util::slist formatstack={},util::slist dialects={});

std::pair<std::list<isis::data::Image>,util::slist>
load_list(util::slist paths,util::slist formatstack={},util::slist dialects={});

data::Image makeImage(py::buffer b, std::map<std::string,util::ValueTypes> metadata);

std::variant<py::none, util::ValueTypes,std::list<util::ValueTypes>> property2python(util::PropertyValue val);
}