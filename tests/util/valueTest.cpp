/*
 * valueTest.cpp
 *
 * A unit test suite to confirm the capabilities of the
 * utils::Value class.
 *
 *  Created on: Sep 24, 2009
 *      Author: proeger
 */

// This might crash on MacOS. See http://old.nabble.com/-Boost.Test--Error:-Non-aligned-pointer-being-freed-td24335733.html
// seems to work with XCode 3.2.3 and boost 1.42

#define BOOST_TEST_MODULE ValueTest
#define NOMINMAX 1
#include <boost/test/unit_test.hpp>
#include <string>
#include <isis/core/value.hpp>
#include <isis/core/vector.hpp>
#include <chrono>
#include <boost/numeric/conversion/converter.hpp>

#include <complex>

namespace isis
{
namespace test
{

using util::ValueNew;
using util::fvector4;
using util::ivector4;

// TestCase object instantiation
BOOST_AUTO_TEST_CASE( test_type_init )
{
	/*  ENABLE_LOG( CoreDebug, util::DefaultMsgPrint, verbose_info );
	    ENABLE_LOG( CoreLog, util::DefaultMsgPrint, verbose_info );*/
	ValueNew tInt( 42 );   // integer
	ValueNew tStr( std::string( "Hello World" ) ); // string
	ValueNew tFlt( (float)3.1415 );
}

// TestCase toString()
BOOST_AUTO_TEST_CASE( type_toString_test )
{
	// create some test dummies
	ValueNew tInt( 42 );
	ValueNew tFloat( (float)3.1415 );
	ValueNew tString( std::string( "Hello World" ) );

	BOOST_CHECK_EQUAL( tInt.toString(), "42" );
	BOOST_CHECK_EQUAL( tFloat.toString(), "3.1415" );
	BOOST_CHECK_EQUAL( tString.toString(), "Hello World" );
}

// TestCase is()
BOOST_AUTO_TEST_CASE( test_type_is )
{
	// see if Values are created as the expected types
	BOOST_CHECK( ValueNew( 42 ).is<int32_t>() );
	BOOST_CHECK( ValueNew( (float)3.1415 ).is<float>());
	BOOST_CHECK( ValueNew( std::string( "Hello World" ) ).is<std::string>());
	BOOST_CHECK( ValueNew( "Hello char World" ).is<std::string>());
}

// TestCase operators()
BOOST_AUTO_TEST_CASE( test_operators )
{
	ValueNew tInt1( 21 ), tInt2( 21 );
	ValueNew _21str("21");
	
	BOOST_CHECK_EQUAL( tInt1 + tInt2, 42 );
	BOOST_CHECK_EQUAL( tInt1 * 2, 42 );
//	BOOST_CHECK_EQUAL( 42 - tInt1, tInt2 ); //@todo maybe add two sided operators

}
BOOST_AUTO_TEST_CASE( test_ext_operators )
{
	ValueNew tInt1( 21 ), tInt2( 21 );
	ValueNew _21str("21"),_2str("2"),_2strneg("-2");

	// proper operation
	BOOST_CHECK_EQUAL( tInt1.plus(_21str), ValueNew( 21+21 ) );
	BOOST_CHECK_EQUAL( tInt1.minus(_21str), ValueNew( 21-21 ) );
	BOOST_CHECK_EQUAL( tInt1.multiply(_21str), ValueNew( 21*21 ) );
	BOOST_CHECK_EQUAL( tInt1.divide(ValueNew("2")), ValueNew( 10 ) ); // int(21/2)=10

	BOOST_CHECK_EQUAL( _2str.plus(ValueNew(1)), _21str ); // "2" + "1" = "21"

	{
		ValueNew buff=tInt1;
		BOOST_REQUIRE_EQUAL( buff.multiply_me(_2str), ValueNew( 42 ) );
		BOOST_REQUIRE_EQUAL( buff.divide_me(_2str),   ValueNew( 21 ) );
		BOOST_REQUIRE_EQUAL( buff.add(_2str),         ValueNew( 23 ) );
		BOOST_CHECK_EQUAL( buff.substract(_21str),    ValueNew( 2 ) );
	}

	BOOST_CHECK(!ValueNew(50).eq(_2strneg));

	// invalid operation should not change value
	BOOST_CHECK_EQUAL(ValueNew((uint16_t)50).plus(_2strneg),ValueNew((uint16_t)50));
}

BOOST_AUTO_TEST_CASE( type_comparison_test )
{
	ValueNew _200( ( uint8_t )200 );
	ValueNew _1000( ( int16_t )1000 );
	ValueNew _200komma4( 200.4f );
	ValueNew _200komma6( 200.6f );
	ValueNew _minus1( ( int16_t ) - 1 );
	ValueNew fucking_much( std::numeric_limits<float>::max() );
	ValueNew even_more( std::numeric_limits<double>::max() );
	// this should use implicit conversion to the inner type and compare them
	BOOST_CHECK( _200 < _1000 );
	BOOST_CHECK( _200 > _minus1 );
	BOOST_CHECK( _200.lt( _1000 ) ) ;
	BOOST_CHECK( _200.gt( _minus1 ) ) ;
	//200.4 will be rounded to 200 - and a message send to CoreDebug
	BOOST_CHECK( ! _200.lt( _200komma4 ) ) ;
	BOOST_CHECK( _200.eq( _200komma4 ) ) ;
	//200.6 will be rounded to 201 - and a message send to CoreDebug
	BOOST_CHECK( _200.lt( _200komma6 ) ) ;
	// compares 200.4 to 200f
	BOOST_CHECK( ! _200komma4.eq( _200 ) ) ;
	BOOST_CHECK( _200komma4.gt( _200 ) ) ;
	// compares 200.4 to 1000i
	BOOST_CHECK( _200komma4.lt( _1000 ) );
	// push the limits
	BOOST_CHECK( _1000.lt( fucking_much ) );
	BOOST_CHECK( fucking_much.gt( _1000 ) );
	BOOST_CHECK( fucking_much.lt( even_more ) );

	ValueNew a(util::Selection("a,b,c","a")),b=a;
	ValueNew other(util::Selection("aa,bb,cc","aa")),unset(util::Selection("x"));

	BOOST_CHECK(  a.eq(b));
	BOOST_CHECK( !a.lt(b));
	BOOST_CHECK( !a.gt(b));

	BOOST_CHECK(!a.gt(other));
	BOOST_CHECK(!a.eq(other));
	BOOST_CHECK(!a.lt(other));
	
	BOOST_CHECK(ValueNew("a").eq(a));
	BOOST_CHECK(ValueNew(" ").lt(a));
	BOOST_CHECK(ValueNew("bb").gt(a));
	// Value<int>(1).eq(a); no conversion Selection=>int available
	// a.eq(Value<std::string>("a")); no conversion String=>Selection available
}

BOOST_AUTO_TEST_CASE( type_conversion_test )
{
	ValueNew tInt( 42 );
	ValueNew tFloat1( 3.1415f );
	ValueNew tFloat2( 3.5415 );
	ValueNew vec( ivector4( {1, 2, 3, 4} ) );
	BOOST_CHECK_EQUAL( tInt.as<double>(), 42 );
	BOOST_CHECK_EQUAL( tFloat1.as<int32_t>(), ( int32_t )ceil( tFloat1.as<float>() - .5 ) );
	BOOST_CHECK_EQUAL( tFloat2.as<int32_t>(), ( int32_t )ceil( tFloat2.as<double>() - .5 ) );
	BOOST_CHECK_EQUAL( tFloat2.as<std::string>(), "3.5415" );
	BOOST_CHECK_EQUAL( vec.as<fvector4>(), fvector4( {1, 2, 3, 4}) );
}

BOOST_AUTO_TEST_CASE( complex_conversion_test )
{
	ValueNew tFloat1( std::complex<float>( 3.5415, 3.5415 ) );
	ValueNew tDouble1( std::complex<double>( 3.5415, 3.5415 ) );

	//because of rounding std::complex<double>(3.5415,3.5415) wont be equal to std::complex<float>(3.5415,3.5415)
	BOOST_CHECK_EQUAL( tFloat1.as<std::complex<double> >(), std::complex<double>( ( float )3.5415, ( float )3.5415 ) );

	BOOST_CHECK_EQUAL( tDouble1.as<std::complex<float> >(), std::complex<float>( 3.5415, 3.5415 ) );

	BOOST_CHECK_EQUAL( tDouble1.as<std::complex<double> >(), std::complex<double>( 3.5415, 3.5415 ) );

	BOOST_CHECK_EQUAL( tFloat1.as<std::string>(), "(3.5415,3.5415)" );
	BOOST_CHECK_EQUAL( ValueNew( "(3.5415,3.5415)" ).as<std::complex<float> >(), std::complex<float>( 3.5415, 3.5415 ) );

	BOOST_CHECK_EQUAL( ValueNew( 3.5415f ).as<std::complex<float> >(), std::complex<float>( 3.5415, 0 ) );
	BOOST_CHECK_EQUAL( ValueNew( -5 ).as<std::complex<float> >(), std::complex<float>( -5, 0 ) );
}

BOOST_AUTO_TEST_CASE( color_conversion_test )
{
	const util::color24 c24 = {10, 20, 30};
	const util::ValueNew vc24 = c24;
	BOOST_CHECK( vc24.is<util::color24>());
	BOOST_CHECK_EQUAL( vc24.as<std::string>(), "{10,20,30}" ); // conversion to string
	BOOST_CHECK_EQUAL( vc24.as<util::color48>(), c24 ); // conversion to 16bit color
}

// TestCase object instantiation
BOOST_AUTO_TEST_CASE( vector_convert_test )
{
	util::fvector3 v1;
	v1.fill( 42.1 );
	util::dvector4 v2 = util::ValueNew( v1 ).as<util::dvector4>();
	util::ivector4 v3 = util::ValueNew( v1 ).as<util::ivector4>();

	for( int i = 0; i < 3; i++ ) {
		BOOST_CHECK_EQUAL( v1[i], v2[i] );
		BOOST_CHECK_EQUAL( ( int )v1[i], v3[i] );
	}
}

BOOST_AUTO_TEST_CASE( from_string_conversion_test )
{
	util::enableLog<util::DefaultMsgPrint>(info);
	// convert a string into a list of strings
	const char *sentence[] = {"This", "is", "a", "sentence"};
	ValueNew sSentence( "This is a sentence" );
	BOOST_CHECK_EQUAL( sSentence.as<util::slist>(), std::list<std::string>( sentence, sentence + 4 ) );

	// convert a string into a list of numbers (pick the numbers, ignore the rest)
	const int inumbers[] = {1, 2, 3, 4, 5};
	const double dnumbers[] = {1, 2, 3, 4.4, 4.6};
	ValueNew sNumbers( "1, 2, 3 and 4.4 or maybe 4.6" );
	const util::color24 col24 = {1, 2, 3};
	const util::color48 col48 = {100, 200, 300};

	BOOST_CHECK_EQUAL( sNumbers.as<util::ilist>(), std::list<int>( inumbers, inumbers + 5 ) ); // 4.4 and 4.6 will be rounded
	BOOST_CHECK_EQUAL( sNumbers.as<util::dlist>(), std::list<double>( dnumbers, dnumbers + 5 ) );

	BOOST_CHECK_EQUAL( util::ValueNew( "<1|2|3>" ).as<util::fvector4>(), util::fvector4( {1, 2, 3} ) ); //should also work for fvector
	BOOST_CHECK_EQUAL( util::ValueNew( "<1|2|3|4|5>" ).as<util::fvector4>(), util::fvector4( {1, 2, 3, 4} ) ); //elements behind end are ignored
	BOOST_CHECK_EQUAL( util::ValueNew( "1,2,3,4,5>" ).as<util::ivector4>(), util::ivector4( {1, 2, 3, 4} ) ); //elements behind end are ignored

	BOOST_CHECK_EQUAL( util::ValueNew( "<1,2,3,4,5>" ).as<util::color24>(), col24 ); //elements behind end are ignored
	BOOST_CHECK_EQUAL( util::ValueNew( "<100,200,300,4,5>" ).as<util::color48>(), col48 ); //elements behind end are ignored

}

BOOST_AUTO_TEST_CASE( time_conversion_test )
{
	util::timestamp the_day_after(std::chrono::hours(24)+std::chrono::milliseconds(1));
	
	std::time_t current_time;
	std::time(&current_time);
	struct std::tm *timeinfo = std::localtime(&current_time);
	
	// strings are parsed in local time - so we need an offset
	BOOST_CHECK_EQUAL(
		util::ValueNew( "19700102000000.001").as<util::timestamp>(),
		the_day_after-std::chrono::seconds(timeinfo->tm_gmtoff)+std::chrono::hours(timeinfo->tm_isdst)
	);
	
	BOOST_CHECK_EQUAL( util::ValueNew( "19700102").as<util::date>(),util::date()+std::chrono::days(1));

	{ //@todo %c is broken
// 	std::ostringstream str;
// 	str << the_day_after;
// 	BOOST_CHECK_EQUAL( util::Value<std::string>( str.str()).as<util::timestamp>(),the_day_after); 
	}

	{
	std::ostringstream str;
	util::timestamp rhs=util::timestamp()+std::chrono::hours(1)+std::chrono::minutes(1);
	str << (rhs);
	util::timestamp lhs=util::ValueNew( str.str()).as<util::timestamp>();
	BOOST_CHECK_EQUAL(lhs, rhs); 
	}
}

}
}
