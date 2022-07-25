//
// Created by reimer on 7/13/22.
//

#include "io_protocol.hpp"

#include <memory>
#include "fileptr.hpp"
#include "io_pluginhandler.hpp"

namespace isis::io{
Generator<IoProtocol::load_result>
protocol_load( const std::string &path, const std::list<util::istring>& formatstack, const std::list<util::istring>& dialects, util::slist* rejected)
{
	std::unique_ptr<io::PrimaryIoProtocol> protocol;//(new io::DirectoryProtocol);
	return protocol_load(protocol->load(path,dialects),formatstack,dialects,rejected);
}
Generator<IoProtocol::load_result>
protocol_load(Generator<IoProtocol::load_result> &&outer_generator, const std::list<util::istring> &formatstack, const std::list<util::istring> &dialects, util::slist *rejected)
{
	while (outer_generator) {
		auto [name, future] = outer_generator();
		std::unique_ptr<io::SecondaryIoProtocol> inner_protocol;
		auto inner_generator = formatstack.size()>1 ?
			protocol_load(inner_protocol->load(std::move(future),dialects),formatstack,dialects,rejected):
			inner_protocol->load(std::move(future),dialects);//terminate recursion

		while (inner_generator)
			co_yield inner_generator();
	}
}
}
