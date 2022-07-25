//
// Created by Enrico Reimer on 24.07.22.
//

#include "ioprotocol_FileDirectory.hpp"
#include <isis/core/fileptr.hpp>

namespace isis::io
{
util::slist IoProtocolFileDirectory::prefixes()
{ return {"file:", ""}; }

Generator<IoProtocol::load_result> IoProtocolFileDirectory::load(std::string path, const std::list<util::istring> &dialects)
{
	if (std::filesystem::is_directory(path)) {
		for (auto f = std::filesystem::directory_iterator(path); f != std::filesystem::directory_iterator(); ++f) {
			if (!f->is_directory())
				co_yield load_result{f->path().native(), make_loader(path)};
		}
	}
	else {
		co_yield load_result{path, make_loader(path)};
	}
}
std::future<IoProtocol::io_object> IoProtocolFileDirectory::make_loader(const std::filesystem::path &file)
{
	std::packaged_task<IoProtocol::io_object()> task(
		[file]() -> io_object
		{
			if (std::filesystem::file_size(file) > mapping_size) {
				return data::FilePtr(file);
			} else {
				std::unique_ptr<std::filebuf> stream(new std::filebuf);
				stream->open(file, std::ios_base::binary | std::ios_base::in);
				return std::move(stream);
			}
		});
	return task.get_future();
}
}

void *init(){return isis::io::IoProtocol::init<isis::io::IoProtocolFileDirectory>();}
const char *name(){return "normal file protocol";}
