/*
 * propertyTest.cpp
 *
 *  Created on: Sep 23, 2009
 *      Author: proeger
 */

// The BOOST_TEST_MODULE is similar to a unit test suite.
#define BOOST_TEST_MODULE PropertyTests
#include <boost/test/unit_test.hpp>
#include <isis/core/property.hpp>
#include <string>
#include <iostream>

namespace isis::test
{
	using util::PropertyValue;

BOOST_AUTO_TEST_CASE( property_init_test )
{
	ENABLE_LOG(CoreLog,util::DefaultMsgPrint,error);
	ENABLE_LOG(CoreDebug,util::DefaultMsgPrint,error);
	//  default constructor
	PropertyValue propZero;
	//  initializer
	PropertyValue propA(std::string( "Property01" ), false);
	BOOST_CHECK_EQUAL( propA.toString(), "Property01" );
	//  default: not needed
	BOOST_CHECK( !propA.isNeeded() );
	BOOST_CHECK( !propZero.isNeeded() );
}

BOOST_AUTO_TEST_CASE( property_copy_test )
{
	// Test copy operator
	PropertyValue propA(( int32_t )5, false);
	PropertyValue propB = propA;
	BOOST_CHECK_EQUAL( propB, ( int32_t )5 );
	//check for deep copy (change of propA shall not change propB)
	propA = ( int32_t )6;
	BOOST_CHECK_EQUAL( propA, ( int32_t )6 );
	BOOST_CHECK_EQUAL( propB, ( int32_t )5 );
}

BOOST_AUTO_TEST_CASE( property_compare_test )
{
	// trivial case - direct compare
	BOOST_CHECK_EQUAL( PropertyValue( 5 ), 5 );
	BOOST_CHECK_NE( PropertyValue( 5.4 ), 5 );

	// compare PropertyValue
	BOOST_CHECK_EQUAL( PropertyValue( 5 ), PropertyValue( 5 ));

	PropertyValue fives16bit,fivesstr,fivesint;
	for(int i=0;i<5;i++){
		fives16bit.push_back((uint16_t)5);
		fivesint.push_back((int)5);
		fivesstr.push_back(std::string("5"));
	}

	BOOST_CHECK( fives16bit.eq(fivesint)); // not the same type
	BOOST_CHECK( fives16bit.eq(fives16bit));

	// length compare
	PropertyValue buff=fives16bit;
	BOOST_REQUIRE_EQUAL(buff,fives16bit);
	buff.push_back((uint16_t)6);
	BOOST_CHECK_NE(buff,fives16bit);
	buff.erase(4);
	BOOST_CHECK_NE(buff,fives16bit); // compares 5,5,5,5,6 and 5,5,5,5,5
	
	BOOST_CHECK(!(PropertyValue()!=PropertyValue())); //empty properies are not unequal
	BOOST_CHECK(!(PropertyValue()==PropertyValue())); //nor are they equal

	// converting compare
	BOOST_CHECK( fives16bit.eq(fivesint));
	BOOST_CHECK( fives16bit.eq(fivesstr));
	
	BOOST_CHECK( fivesint.eq(fives16bit));
	BOOST_CHECK( fivesint.eq(fivesstr));
	
	BOOST_CHECK( fivesstr.eq(fivesint));
	BOOST_CHECK( fivesstr.eq(fives16bit));
	
}

BOOST_AUTO_TEST_CASE( property_operator_test )
{
	PropertyValue fivesint,tensint,twosint,sixesint;
	for(size_t i=0;i<5;i++){
		twosint.push_back(2);
		fivesint.push_back(5);
		sixesint.push_back(6);
		tensint.push_back(10);
	}
	
	BOOST_CHECK_EQUAL(fivesint.plus(1),sixesint); //plus operation with single value
	BOOST_CHECK_EQUAL(fivesint.plus(fivesint),tensint); //plus operation with list

	BOOST_CHECK_EQUAL(tensint.minus(fivesint),fivesint); //minus operation with list
	BOOST_CHECK_EQUAL(sixesint.minus(1),fivesint); //minus operation with single value

	BOOST_CHECK_EQUAL(fivesint.multiply(twosint),tensint);
	BOOST_CHECK_EQUAL(tensint.divide(twosint),fivesint);
}

BOOST_AUTO_TEST_CASE( direct_property_operator_test )
{
	BOOST_CHECK_EQUAL(PropertyValue(5)*2,10);
	BOOST_CHECK_EQUAL(PropertyValue(5)*2+5,15);
	BOOST_CHECK_EQUAL(PropertyValue(5)*.5,2.5);//operatorX() changes type if necessary
	
	BOOST_CHECK_EQUAL(PropertyValue(5)/2,2);
	BOOST_CHECK_EQUAL(PropertyValue(5)/2+5,7);
	BOOST_CHECK_EQUAL(PropertyValue(5)/2.f+5,7.5);
	BOOST_CHECK_EQUAL(PropertyValue(5)/5,1);
	
	BOOST_CHECK_EQUAL(PropertyValue(5)+2,7);

	BOOST_CHECK_EQUAL(PropertyValue(5)-2,3);
	BOOST_CHECK_EQUAL(PropertyValue(5)-.5,4.5);
	
}

BOOST_AUTO_TEST_CASE( property_list_creation )
{
	const util::Value buff[]={0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	PropertyValue list;
	list.insert(list.end(),buff,buff+10);
	for(int i=0;i<10;i++){
		BOOST_CHECK_EQUAL(list[i],buff[i]);
	}
}

BOOST_AUTO_TEST_CASE( property_list_splice )
{
	const util::Value buff[]={0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
	PropertyValue list;
	list.insert(list.end(),buff,buff+10);

	std::vector< PropertyValue > vec=list.splice(4);
	BOOST_CHECK_EQUAL(vec[0].size(),10*4/10); //10 over (10 over 4) equal 10*4 over 10
	BOOST_CHECK_EQUAL(vec[1].size(),10*4/10);
	BOOST_CHECK_EQUAL(vec[2].size(),10%4); // remainder
	for(int i=0;i<10;i++)
			BOOST_CHECK_EQUAL(vec[i/4][i%4],buff[i]);
		
}

}
