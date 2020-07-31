#define BOOST_TEST_MODULE ValueTest
#define NOMINMAX 1
#include <boost/test/unit_test.hpp>
#include <isis/core/istring.hpp>
#include <isis/core/common.hpp>

namespace isis::test
{

// TestCase object instantiation
BOOST_AUTO_TEST_CASE( istring_test )
{
	BOOST_CHECK_EQUAL( util::istring( "HaLLo there" ), util::istring( "Hallo thERe" ) );
	BOOST_CHECK( util::istring( "HaLLo there1" ) != util::istring( "Hallo thERe" ) );
	BOOST_CHECK_EQUAL( util::istring( "Hallo thERe" ).find( "there" ), 6 );

	int buffer;
	BOOST_REQUIRE(util::stringTo(util::istring( "1234" ),buffer));
	BOOST_CHECK_EQUAL(buffer , 1234 );

	std::complex<double> buffer_c;
	BOOST_REQUIRE(util::stringTo(std::string("(12,34)"),buffer_c));
	BOOST_CHECK_EQUAL(buffer_c, std::complex<double>(12,34));

	util::istring buffer_i;
	BOOST_REQUIRE(util::stringTo(std::string( "Test" ),buffer_i));
	BOOST_CHECK_EQUAL( buffer_i, "Test" );

	std::string buffer_s;
	BOOST_REQUIRE(util::stringTo(util::istring( "Test" ),buffer_s));
	BOOST_CHECK_EQUAL( buffer_s, "Test" );
}

}
