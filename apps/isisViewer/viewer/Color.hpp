#ifndef COLOR_HPP
#define COLOR_HPP

#include "common.hpp"
#include <QColor>

namespace isis
{
namespace viewer
{


class Color
{

public:
	enum LookUpTableType { hsvLUT, hsvLUT_reverse, wbryw };
	static std::vector< util::fvector4 > getColorGradientRGB( const LookUpTableType &lutType, const size_t &numberOfEntries );


};

}
}





#endif