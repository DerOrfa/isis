//
// Created by Enrico Reimer on 24.07.22.
//

#pragma once
#include <isis/core/io_protocol.hpp>

namespace isis::io
{
class IoProtocolFileDirectory: public PrimaryIoProtocol
{
	static const size_t mapping_size = 1024 * 1024 * 10;
	static std::future<io_object> make_loader(const std::filesystem::path &file);
public:
	util::slist prefixes() override;
	Generator<load_result> load(std::string path, const std::list<util::istring> &dialects) override;
};
}
