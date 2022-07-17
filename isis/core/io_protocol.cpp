//
// Created by reimer on 7/13/22.
//

#include "io_protocol.hpp"
#include "fileptr.hpp"
#include <fstream>

namespace isis::io{

util::slist DirectoryProtocol::prefixes(){return {"file:"};}
util::slist DirectoryProtocol::suffixes(){return {};}

Generator<IoProtocol::load_result> DirectoryProtocol::load(std::string path)
{
	for(auto f=std::filesystem::directory_iterator( path );f!=std::filesystem::directory_iterator();++f)
	{
		if(f->is_directory())continue;
		if(f->file_size()>mapping_size) {
			data::FilePtr memory(f->path());
			co_yield load_result{f->path().native(),memory};
		} else {
			std::filebuf stream;
			stream.open(f->path(),std::ios_base::binary|std::ios_base::in);
			co_yield load_result{f->path().native(),std::move(stream)};
		}
	}
}
}
