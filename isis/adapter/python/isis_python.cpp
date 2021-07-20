//
// Created by enrico on 11.07.21.
//

#include "pybind11/pybind11.h"
#include "pybind11/chrono.h"
#include "pybind11/stl.h"
#include "../../core/image.hpp"
#include "utils.hpp"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace pybind11::literals;
using namespace isis;

auto cleanup_callback = []() {
	python::free_logging(verbose_info);
};


PYBIND11_MODULE(isis, m)
{
python::setup_logging();
m.add_object("_cleanup", py::capsule(cleanup_callback));

py::class_<data::Chunk>(m, "chunk")
	.def("__array__",[](data::Chunk &ch){return python::make_array(ch);	})
	.def("__getitem__", [](const data::Chunk &ch, std::string path){
		return python::property2python(ch.property(path.c_str()));
	})
;
py::class_<data::Image>(m, "image")
	.def("__array__",[](data::Image &img){return python::make_array(img);	})
	.def("getChunks",
		 [](const data::Image &img, bool copy_metadata) {return img.copyChunksToVector(copy_metadata);	},
		 "get all chunks of pixel data that make the image",
		 py::arg("copy_metadata")=false
	)
	.def("__getitem__", [](const data::Image &img, std::string path){
		return python::property2python(img.property(path.c_str()));
	})
	.def("__contains__", [](const data::Image &img, std::string path)->bool{return img.hasProperty(path.c_str());})
	.def("__repr__", [](const data::Image &img) {
		return img.identify(false,false)+" " + img.getSizeAsString() + " " + img.getMajorTypeName();
	})
	.def_property_readonly("shape", &data::Image::getSizeAsVector)
	.def("identify", &data::Image::identify,"withpath"_a=true,"withdate"_a=false)
	.def("getMetaData", [](const data::Image &img) {
		std::map<std::string,std::variant<py::none, util::ValueTypes, std::list<util::ValueTypes>>> ret;
		for(auto &set:img.getFlatMap()){
			ret.emplace(set.first.toString(),python::property2python(set.second));
		}
		return ret;
	})
;

m.def("load",&python::load,
	  "filepath"_a,
	  "formatstack"_a=util::slist{},
	  "dialects"_a=util::slist{}
);
m.def("load",&python::load_list,
	  "filepaths"_a,
	  "formatstack"_a=util::slist{},
	  "dialects"_a=util::slist{}
);
}
