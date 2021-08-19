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

PYBIND11_MODULE(pyisis, m)
{

python::setup_logging();
m.add_object("_cleanup", py::capsule([]() {python::free_logging(verbose_info);}));

m.def("setLoglevel", [](const char *level,const char *module)->void{
	util::Selection rq_level({"error", "warning", "notice", "info", "verbose_info"});
	if(!rq_level.set(level)){
		LOG(python::Debug,error) << "Not setting invalid logging level " << level << " for " << module;
	}
	python::setLogging(rq_level,module);
},"level"_a="notice","module"_a="all");

py::class_<data::ValueArray>(m, "ValueArray");
py::class_<data::NDimensional<4>>(m, "4Dimensional");
py::class_<util::PropertyMap>(m, "PropertyMap")
	.def("keys", [](const util::PropertyMap &ob) {
		std::list<std::string> ret;
		for(auto &set:ob.getFlatMap()){
			ret.push_back(set.first.toString());
		}
		return ret;
	})
	.def("__contains__", [](const util::PropertyMap &ob, std::string path)->bool{return ob.hasProperty(path.c_str());})
	.def("__getitem__", [](const util::PropertyMap &ob, std::string path){
		return python::property2python(ob.property(path.c_str()));
	})
	.def("getMetaData", [](const util::PropertyMap &ob) {
		std::map<std::string,std::variant<py::none, util::ValueTypes, std::list<util::ValueTypes>>> ret;
		for(auto &set:ob.getFlatMap()){
			ret.emplace(set.first.toString(),python::property2python(set.second));
		}
		return ret;
	})
;


py::class_<data::Chunk, data::ValueArray, data::NDimensional<4>, util::PropertyMap>(m, "chunk")
	.def_property_readonly("nparray",[](data::Chunk &chk){return python::make_array(chk);	})
	.def("__array__",[](data::Chunk &ch){return python::make_array(ch);	})
;
py::class_<data::Image, data::NDimensional<4>, util::PropertyMap>(m, "image")
	.def(py::init(&python::makeImage))
	.def("__array__",[](data::Image &img){return python::make_array(img);	})
	.def_property_readonly("nparray",[](data::Image &img){return python::make_array(img);	})
	.def("getChunks",
		 [](const data::Image &img, bool copy_metadata) {return img.copyChunksToVector(copy_metadata);	},
		 "get all chunks of pixel data that make the image",
		 py::arg("copy_metadata")=false
	)
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
