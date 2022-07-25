//
// Created by Enrico Reimer on 24.07.22.
//

#include "ioprotocol_FormatAdaptor.hpp"
#include <isis/core/io_factory.hpp>

namespace isis::io
{
namespace _internal{
struct load_source_maker{
	data::IOFactory::load_source operator()(std::unique_ptr<std::streambuf> &filebuf){
		return filebuf.get();
	}
	data::IOFactory::load_source operator()(data::ByteArray &b_array){
		return b_array;
	}
	data::IOFactory::load_source operator()(auto &fail){
		assert(false);
		return (std::streambuf*)(nullptr);
	}
};
}

Generator<IoProtocol::load_result>
FileFormatAdapter::load(std::future<io_object> obj, const std::list<util::istring> &dialects)
{
	auto processed = std::move(obj.get());
	data::IOFactory::load_source src = std::visit(_internal::load_source_maker(), processed);
	data::IOFactory::load(src);
}
util::slist FileFormatAdapter::suffixes()
{
	return {};
}
}

void *init(){return isis::io::IoProtocol::init<isis::io::FileFormatAdapter>();}
const char *name(){return "fileformat adapter";}
