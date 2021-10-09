#pragma once

#include "../../core/chunk.hpp"


namespace isis{
namespace math{
namespace cl{

bool fft(data::TypedChunk<std::complex<float > > &data, bool inverse, float scale=0);

}
}
}



