//
// Created by enrico on 11.07.21.
//

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "pybind11/complex.h"
#include "../../core/image.hpp"
#include "utils.hpp"

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;
using namespace pybind11::literals;
using namespace isis;

PYBIND11_MODULE(isis, m)
{
py::class_<data::Chunk>(m, "chunk", py::buffer_protocol())
	.def_buffer([](const data::Chunk &ch)->py::buffer_info{return python::make_buffer(ch);	})
	.def("__getitem__", [](const data::Chunk &ch, std::string path) {return ch.property(path.c_str());	});

py::class_<data::Image>(m, "image", py::buffer_protocol())
	.def("getChunks",
		 [](const data::Image &img, bool copy_metadata) {return img.copyChunksToVector(copy_metadata);	},
		 "get all chunks of pixel data that make the image",
		 py::arg("copy_metadata")=false
	)
	.def("__getitem__", [](const data::Image &img, std::string path) {return img.property(path.c_str());	});

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
