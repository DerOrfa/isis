//
// Created by enrico on 11.07.21.
//

#include "pybind11/pybind11.h"
#include "pybind11/chrono.h"
#include "pybind11/stl.h"
#include "../../core/image.hpp"
#include "utils.hpp"
#include "logging.hpp"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace pybind11::literals;
using namespace isis;

PYBIND11_MODULE(isis, m)
{

python::setup_logging();
m.add_object("_cleanup", py::capsule([]() {python::free_logging(verbose_info);}));

m.def("setLoglevel", [](const char *level,const char *module)->void{
	util::Selection rq_level({"error", "warning", "notice", "info", "verbose_info"});
	if(!rq_level.set(level))
		LOG(python::Debug,error) << "Not setting invalid logging level " << level << " for " << module;
	python::setLogging(rq_level,module);
},"level"_a="notice","module"_a="all");

py::class_<data::Chunk>(m, "chunk")
	.def_property_readonly("nparray",[](data::Chunk &chk){return python::make_array(chk);	})
	.def("__array__",[](data::Chunk &ch){return python::make_array(ch);	})
	.def("__getitem__", [](const data::Chunk &ch, std::string path){
		return python::property2python(ch.property(path.c_str()));
	})
;
py::class_<data::Image>(m, "image")
	.def(py::init(&python::makeImage))
	.def("__array__",[](data::Image &img){return python::make_array(img);	})
	.def_property_readonly("nparray",[](data::Image &img){return python::make_array(img);	})
	.def("getChunks",
		 [](const data::Image &img, bool copy_metadata) {return img.copyChunksToVector(copy_metadata);	},
		 "get all chunks of pixel data that make the image",
		 py::arg("copy_metadata")=false
	)
	.def("keys", [](const data::Image &img) {
		std::list<std::string> ret;
		for(auto &set:img.getFlatMap()){
			ret.push_back(set.first.toString());
		}
		return ret;
	})
	.def("__getitem__", [](const data::Image &img, std::string path){
		return python::property2python(img.property(path.c_str()));
	})
	.def("__contains__", [](const data::Image &img, std::string path)->bool{return img.hasProperty(path.c_str());})
	.def("__repr__", [](const data::Image &img) {
		return img.identify(false,false)+" " + img.getSizeAsString() + " " + img.getMajorTypeName();
	})
	.def_property_readonly("shape", &data::Image::getSizeAsVector)
	.def("identify", &data::Image::identify,"withpath"_a=true,"withdate"_a=false)
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
