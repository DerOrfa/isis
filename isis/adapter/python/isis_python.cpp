//
// Created by enrico on 11.07.21.
//

#include "pybind11/pybind11.h"
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
	static const std::map<LogLevel,std::string> severity_map={
		{error,"error"},
		{warning,"warning"},
		{notice,"notice"},
		{info,"info"},
		{verbose_info,"verbose"}
	};
	util::Selection rq_level(severity_map,notice);

	if(!rq_level.set(level)){
		LOG(python::Debug,error) << "Not setting invalid logging level " << level << " for " << module;
	}
	python::setLogging(rq_level,module);
},"level"_a="notice","module"_a="all");

py::class_<data::ValueArray>(m, "ValueArray");
py::class_<data::NDimensional<4>>(m, "4Dimensional")
	.def_property_readonly("shape",&data::NDimensional<4>::getSizeAsVector)
	.def_property_readonly("relevantDims",&data::NDimensional<4>::getRelevantDims)
;
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
		return python::property2object(ob.property(path.c_str()));
	})
	.def("getMetaData", &python::getMetaDataFromPropertyMap)
;


py::class_<data::Chunk, data::ValueArray, data::NDimensional<4>, util::PropertyMap>(m, "chunk")
	.def(py::init([](const data::Chunk &chk){return data::Chunk(chk);}))
	.def_property_readonly("nparray",[](data::Chunk &chk){return python::make_array(chk);})
	.def("__array__",[](data::Chunk &ch){return python::make_array(ch);})
;
py::class_<data::Image, data::NDimensional<4>, util::PropertyMap>(m, "image")
	.def(py::init(&python::makeImage))
	.def(py::init([](const data::Image &img){return data::Image(img);}))
	.def(py::init([](const data::Chunk &chk){return data::Image(chk);}))
	.def("__array__",[](data::Image &img){return python::make_array(img);})
	.def_property_readonly("nparray",[](data::Image &img){return python::make_array(img);})
	.def("write",[](const data::Image &img, std::string path, util::slist sFormatstack,util::slist sDialects, py::object repn){
			 return python::write({img},path,sFormatstack,sDialects,repn);
		 },
		 "Write the image as file. If not given, formatstack will be deduced from filename.",
		 "filepath"_a,
		 "formatstack"_a=util::slist{},
		 "dialects"_a=util::slist{},
		 "repn"_a=py::none()
	)
	.def("getChunks",
		 [](const data::Image &img, bool copy_metadata) {return img.copyChunksToVector(copy_metadata);	},
		 "get all chunks of pixel data that make the image",
		 py::arg("copy_metadata")=true
	)
	.def("getChunk",
	 	[](const data::Image &img, py::args tCoords) {
			util::vector4<size_t> coords{0,0,0,0};
			std::transform(tCoords.begin(),tCoords.end(),coords.begin(),[](const py::handle &c){return c.cast<size_t>();});
			LOG(python::Debug,info) << "Returning chunk at " << coords;
			return img.getChunk(coords[1],coords[0],coords[2],coords[3]);//numpy defaults to row major
		},
		"get the chunk at the given image coordinates (row,column,slice,time)"
	)
	.def("__repr__", [](const data::Image &img) {
		return img.identify(false,false)+" " + img.getSizeAsString() + " " + img.getMajorTypeName();
	})
	.def_property_readonly("shape", &data::Image::getSizeAsVector)
	.def("identify", &data::Image::identify,"withpath"_a=true,"withdate"_a=false)
	.def(
		"getMetaData",
		&python::getMetaDataFromImage,
		"creates a flat dictionary of all properties of the image including those of all chunks if requested (default: True)",
		"merge_chunk_data"_a=true)
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
m.def("write",&python::write,
	  "Write images as file(s). If not given, formatstack will be deduced from filename.",
	  "images"_a,
	  "filepath"_a,
	  "formatstack"_a=util::slist{},
	  "dialects"_a=util::slist{},
	  "repn"_a=py::none()
);
}
