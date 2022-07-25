//
// Created by Enrico Reimer on 24.07.22.
//

#pragma once
#include <isis/core/io_protocol.hpp>

namespace isis::io
{
class FileFormatAdapter: public SecondaryIoProtocol
{
public:
	Generator<load_result> load(std::future<io_object> obj, const std::list<util::istring> &dialects) override;
	util::slist suffixes() override;
};
}
