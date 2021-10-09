
#define BOOST_TEST_MODULE SelectionTest
#include <boost/test/unit_test.hpp>
#include <string>
#include <isis/core/selection.hpp>
#include <isis/core/stringop.hpp>

namespace isis::test
{

BOOST_AUTO_TEST_CASE( test_selection_init )
{
	util::Selection sel({"Val1", "Val2", "Val3"} );
	BOOST_CHECK( !sel ); //undefined, should be false
	BOOST_CHECK_THROW( static_cast<int>(sel), std::bad_optional_access); //trying to use undefined Selection should throw
	BOOST_CHECK_EQUAL( sel.getEntries().size(), 3 );
	BOOST_CHECK_EQUAL( sel.getEntries(), util::stringToList<util::istring>( "Val1,Val2,Val3"s, ',' ) );
}
BOOST_AUTO_TEST_CASE( test_selection_set )
{
	util::Selection sel( {"Val1", "Val2", "Val3"} );
	BOOST_CHECK( ! sel.set( "Val" ) ); //should fail
	BOOST_CHECK( sel.set( "Val1" ) ); //should NOT fail
	BOOST_CHECK_EQUAL( sel, "Val1" );
	BOOST_CHECK_EQUAL( static_cast<int>(sel), 0 );

	BOOST_CHECK( ! sel.idExists( 3 ) ); //should fail
	BOOST_CHECK( sel.idExists( 2 ) ); //should NOT fail

}
BOOST_AUTO_TEST_CASE( test_selection_copy )
{
	util::Selection sel( {"Val1", "Val2", "Val3"} );
	util::Selection copy = sel;
	BOOST_CHECK( !copy.set( "Val" ) ); //should fail
	BOOST_CHECK( copy.set( "Val1" ) ); //should NOT fail
	BOOST_CHECK_EQUAL( copy, "Val1" );
	BOOST_CHECK_THROW( static_cast<int>(sel), std::bad_optional_access); //sel should still be undefined
	sel = copy;// not anymore
	BOOST_CHECK_EQUAL( sel, "Val1" );
	BOOST_CHECK_EQUAL( static_cast<int>(sel), 0 );
}
}
