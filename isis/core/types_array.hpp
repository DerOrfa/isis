#pragma once

#include "types.hpp"
#include "color.hpp"

namespace isis::data{

typedef std::variant<
  std::shared_ptr<bool>
, std::shared_ptr<int8_t>, std::shared_ptr<uint8_t>
, std::shared_ptr<int16_t>, std::shared_ptr<uint16_t>
, std::shared_ptr<int32_t>, std::shared_ptr<uint32_t>
, std::shared_ptr<int64_t>, std::shared_ptr<uint64_t>
, std::shared_ptr<float>, std::shared_ptr<double>
, std::shared_ptr<util::fvector3>, std::shared_ptr<util::fvector4>
, std::shared_ptr<util::dvector3>, std::shared_ptr<util::dvector4>
, std::shared_ptr<util::ivector3>, std::shared_ptr<util::ivector4>
, std::shared_ptr<std::complex<float>>, std::shared_ptr<std::complex<double>>
, std::shared_ptr<util::color24>, std::shared_ptr<util::color48>
> ArrayTypes;

/// @cond _internal
namespace _internal{
struct arrayname_visitor{
	template<typename T> std::string operator()(const std::shared_ptr<T> &)const{
        return util::_internal::name_visitor()(T());
	}
};
/// @encond _internal

}
}

