#include <isis/core/image.hpp>
#include <isis/core/io_factory.hpp>
#include <isis/core/log.hpp>
#include <isis/core/tmpfile.hpp>

#define BOOST_TEST_MODULE "imageIOVistaTest"
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <iostream>
#include <string>

namespace isis::test
{

BOOST_AUTO_TEST_SUITE ( imageIOVista_NullTests )

BOOST_AUTO_TEST_CASE( loadsaveNullImage )
{
	image_io::enableLog<util::DefaultMsgPrint>(warning);
	std::list<data::Image> images = data::IOFactory::load( "nix.null" );
	BOOST_REQUIRE( images.size() >= 1 );
	for( data::Image & null :  images ) {
		switch(null.getMajorTypeID()){ //skip non-supported types
			case util::typeID<uint32_t>():
			case util::typeID<uint64_t>():
			case util::typeID<int64_t>():
			case util::typeID<util::fvector3>():
			case util::typeID<util::fvector4>():
			case util::typeID<util::dvector3>():
			case util::typeID<util::dvector4>():
			case util::typeID<util::ivector3>():
			case util::typeID<util::ivector4>():
			case util::typeID<std::complex<float>>():
			case util::typeID<std::complex<double>>():
				break;
			default:
				util::TmpFile vfile( "", ".v" );
				BOOST_REQUIRE( data::IOFactory::write( null, vfile.native() ) );
				break;
		}
	}
}
BOOST_AUTO_TEST_SUITE_END()
}
