/*
 * ValueArrayTest.cpp
 *
 *  Created on: Sep 25, 2009
 *      Author: proeger
 */

#define NOMINMAX 1
#define BOOST_TEST_MODULE ValueArrayTest
#include <boost/test/unit_test.hpp>
#include <isis/core/valuearray.hpp>
#include <isis/core/valuearray_typed.hpp>
#include <cmath>


namespace isis
{
namespace test
{

namespace _internal{
struct Randomizer{
	size_t length;
	double rnd(){return double(std::rand())/RAND_MAX;}
	template<typename T, size_t NUM> void set(std::array<T,NUM> *ptr,double fact){
		constexpr T max = std::numeric_limits<T>::max();
		for(size_t i=0;i<NUM;i++)
			 (*ptr)[i]= fact == 1 ? max : T(max * fact);
	}
	template<typename T> void set(util::color<T> *ptr,double fact){
		constexpr T max = std::numeric_limits<T>::max();
		ptr->r = fact == 1 ? max : T(max * fact);
		ptr->g = fact == 1 ? max : T(max * fact);
		ptr->b = fact == 1 ? max : T(max * fact);
	}
	template<typename T> void set(std::complex<T> *ptr,double fact){
		const T max = std::sqrt(std::numeric_limits<T>::max());
		*ptr = fact == 1 ?
		    std::polar<T>(max,0):
		    std::polar<T>(max * fact,M_PI * fact * 2);
	}
	template<typename T> void set(T* ptr, double fact, std::enable_if_t<std::is_arithmetic_v<T>> *p=nullptr){
		constexpr T max = std::numeric_limits<T>::max();
		*ptr = fact == 1 ? max : T(max*fact);
	}
	template<typename T> void operator()(std::shared_ptr<T> ptr){
		for(size_t i=0;i<length;i++)
			set(ptr.get()+i,rnd()) ;
		set(ptr.get()+(int)std::floor(rnd()*length),1);
		set(ptr.get()+(int)std::floor(rnd()*length),0);
	}
};
struct Maxer{
	template<typename T, size_t NUM> std::pair<util::Value, util::Value> minmax(std::array<T, NUM> *){
		return {0,std::numeric_limits<T>::max()};
	}
	template<typename T> std::pair<util::Value, util::Value> minmax(util::color<T> *){
		const auto limit=std::numeric_limits<T>::max();
		return {util::color<T>(),util::color<T>{limit,limit,limit}};
	}
	template<typename T> std::pair<util::Value, util::Value> minmax(std::complex<T> *){
		return {0,std::sqrt(std::numeric_limits<T>::max())};
	}
	template<typename T> std::pair<util::Value, util::Value> minmax(T*, std::enable_if_t<std::is_arithmetic_v<T>> *p=nullptr){
		return {0,std::numeric_limits<T>::max()};
	}
	template<typename T> std::pair<util::Value, util::Value> operator()(std::shared_ptr<T> ptr){
		return minmax(ptr.get());
	}
};

struct Deleter {
	static bool deleted;
	void operator()( void *p ) {
		free( p );
		deleted = true;
	};
};
}


// Handlers must not be local classes
class TestHandler : public util::MessageHandlerBase
{
public:
	static int hit;
	TestHandler( LogLevel level ): util::MessageHandlerBase( level ) {}
	virtual ~TestHandler() {}
	void commit( const util::Message &mesg ) {
		if ( mesg.str() == "Automatic numeric conversion of {s} to u16bit failed: bad numeric conversion: negative overflow" )
			hit++;
		else
			std::cout << "Unexpected error " << mesg.merge("");
	}
};
int TestHandler::hit = 0;

bool _internal::Deleter::deleted = false;

BOOST_AUTO_TEST_CASE( scaling_pair_test ){
// 	ENABLE_LOG(DataLog,util::DefaultMsgPrint,verbose_info);
// 	ENABLE_LOG(DataDebug,util::DefaultMsgPrint,verbose_info);
//
	// default constructed scaling_pair must be invalid
	BOOST_CHECK( !data::scaling_pair().valid );
	// 1/0-scaling_pair must be irrelevant
	BOOST_CHECK( !data::scaling_pair(1,0).isRelevant() );
	// 1.5/0-scaling_pair must be relevant
	BOOST_CHECK( data::scaling_pair(1.5,0).isRelevant() );
	// 1/5-scaling_pair must be relevant
	BOOST_CHECK( data::scaling_pair(1,5).isRelevant() );
}

BOOST_AUTO_TEST_CASE( ValueArray_init_test )
{
	BOOST_CHECK( ! _internal::Deleter::deleted );
	{
		data::enableLog<util::DefaultMsgPrint>( error );
		data::TypedArray<int32_t> outer;
		data::enableLog<util::DefaultMsgPrint>( warning );
		// must create an empty pointer
		BOOST_CHECK_EQUAL( outer.getLength(), 0 );
		BOOST_CHECK( ! outer.castTo<int32_t>() ); //the underlying pointer should evaluate to false (as its equal to nullptr);
		BOOST_CHECK_EQUAL( outer.castTo<int32_t>().use_count(), 0 );
		{
			data::ValueArray inner(( int32_t * )calloc(5, sizeof( int32_t ) ), 5, _internal::Deleter() );
			// for now we have only one pointer referencing the data
			auto &dummy = inner.castTo<int32_t>(); //get the smart_pointer inside, because ValueArray does not have/need use_count
			BOOST_CHECK_EQUAL( dummy.use_count(), 1 );
			outer = inner;//now we have two
			BOOST_CHECK_EQUAL( dummy.use_count(), 2 );
			//and both reference the same data
			BOOST_CHECK_EQUAL( outer.getLength(), inner.getLength() );
			BOOST_CHECK_EQUAL( outer.castTo<int32_t>().get(), inner.castTo<int32_t>().get() );
		}
		//now again its only one (inner is gone)
		auto &dummy = outer.castTo<int32_t>();
		BOOST_CHECK_EQUAL( dummy.use_count(), 1 );
	}
	//data should be deleted by now (outer is gone)
	BOOST_CHECK( _internal::Deleter::deleted );
}

BOOST_AUTO_TEST_CASE( ValueArray_minmax_test ){
	_internal::Randomizer randomizer{50};
	for(const auto &t:util::getTypeMap(true)) {
		if(t.first==util::typeID<bool>())
			continue;
		auto array=data::ValueArray::createByID(t.first, randomizer.length);
		array.visit(randomizer);
		std::cout << array.typeName() << std::endl;
		BOOST_CHECK_EQUAL(array.getMinMax(),array.visit(_internal::Maxer()));
	}
}

BOOST_AUTO_TEST_CASE( ValueArray_clone_test )
{
	_internal::Deleter::deleted = false;
	{
		data::ValueArray outer;
		{
			data::ValueArray inner(( int32_t * )calloc(5, sizeof( int32_t ) ), 5, _internal::Deleter() );
			// for now we have only one ValueArray referencing the data
			std::shared_ptr<int32_t> &dummy = inner.castTo<int32_t>(); //get the smart_pointer inside, because ValueArray does not have/need use_count
			BOOST_CHECK_EQUAL( dummy.use_count(), 1 );
			outer = inner;//now we have two
			BOOST_CHECK_EQUAL( dummy.use_count(), 2 );
			//and both reference the same data
			BOOST_CHECK_EQUAL( outer.getLength(), inner.getLength() );
			BOOST_CHECK_EQUAL(
			    outer.castTo<int32_t>().get(),
			    inner.castTo<int32_t>().get()
			);
		}
		//now again its only one (inner is gone)
		std::shared_ptr<int32_t> &dummy = outer.castTo<int32_t>();
		BOOST_CHECK_EQUAL( dummy.use_count(), 1 );
	}
	//data should be deleted by now (outer is gone)
	BOOST_CHECK( _internal::Deleter::deleted );
}

BOOST_AUTO_TEST_CASE( ValueArray_splice_test )
{
	_internal::Deleter::deleted = false;
	{
		std::vector<data::ValueArray> outer;
		{
			data::ValueArray inner(( int32_t * )calloc(5, sizeof( int32_t ) ), 5, _internal::Deleter() );
			// for now we have only one pointer referencing the data
			std::shared_ptr<int32_t> &dummy = inner.castTo<int32_t>(); //get the smart_pointer inside, because ValueArray does not have/need use_count
			BOOST_CHECK_EQUAL( dummy.use_count(), 1 );
			//splicing up makes a references for every spliceAt
			outer = inner.splice( 2 );
			BOOST_CHECK_EQUAL( outer.size(), 5 / 2 + 1 );// 5/2 normal splices plus one halve  spliceAt
			//there shall be outer.size() references from the splices, plus one for the origin
			BOOST_CHECK_EQUAL( dummy.use_count(), outer.size() + 1 );
		}
		BOOST_CHECK_EQUAL( outer.front().getLength(), 2 );// the first slices shall be of the size 2
		BOOST_CHECK_EQUAL( outer.back().getLength(), 1 );// the last slice shall be of the size 1 (5%2)
		//we cannot ask for the use_count of the original because its hidden in DelProxy (outer[0].use_count will get the use_count of the spliceAt)
		//but we can check if it was allready deleted (it shouldn't, because the splices are still using that data)
		BOOST_CHECK( ! _internal::Deleter::deleted );
	}
	//now that all splices are gone the original data shall be deleted
	BOOST_CHECK( _internal::Deleter::deleted );
}

BOOST_AUTO_TEST_CASE( ValueArray_conv_scaling_test )
{
	const float init[] = { -2, -1.8, -1.5, -1.3, -0.6, -0.2, 2, 1.8, 1.5, 1.3, 0.6, 0.2};
	data::ValueArray floatArray=data::ValueArray::make<float>(12 );

	//scaling to itself should allways be 1/0
	floatArray.copyFromMem( init, 12 );
	data::scaling_pair scale = floatArray.getScalingTo( util::typeID<float>() );
	BOOST_CHECK_EQUAL( scale.scale.as<float>(), 1 );
	BOOST_CHECK_EQUAL( scale.offset.as<float>(), 0 );

	//float=> integer should upscale
	scale = floatArray.getScalingTo( util::typeID<int32_t>() );
	BOOST_CHECK_EQUAL( scale.scale.as<double>(), std::numeric_limits<int32_t>::max() / 2. );
	BOOST_CHECK_EQUAL( scale.offset.as<double>(), 0 );

	//float=> unsigned integer should upscale and shift
	scale = floatArray.getScalingTo( util::typeID<uint8_t>() );
	BOOST_CHECK_EQUAL( scale.scale.as<double>(), std::numeric_limits<uint8_t>::max() / 4. );
	BOOST_CHECK_EQUAL( scale.offset.as<double>(), 2 * scale.scale.as<double>() );
}

BOOST_AUTO_TEST_CASE( ValueArray_conversion_test )
{
	const float init[] = { -2, -1.8, -1.5, -1.3, -0.6, -0.2, 2, 1.8, 1.5, 1.3, 0.6, 0.2};
	data::TypedArray<float> floatArray=data::ValueArray::make<float>(12);
	//with automatic upscaling into integer
	floatArray.copyFromMem( init, 12 );

	for ( int i = 0; i < 12; i++ )
		BOOST_REQUIRE_EQUAL( floatArray[i], init[i] );

	data::TypedArray<int32_t> intArray = floatArray.copyAs<int32_t>();
	double scale = std::numeric_limits<int32_t>::max() / 2.;

	for ( int i = 0; i < 12; i++ ) // Conversion from float to integer will scale up to maximum, to map the fraction as exact as possible
		BOOST_CHECK_EQUAL( intArray[i], ceil( init[i]*scale - .5 ) );

	//with automatic downscaling because of range overflow
	scale = std::min( std::numeric_limits< short >::max() / 2e5, std::numeric_limits< short >::min() / -2e5 );

	for ( int i = 0; i < 12; i++ )
		floatArray[i] = init[i] * 1e5;

	data::enableLog<util::DefaultMsgPrint>( error );
	data::TypedArray<short> shortArray = floatArray.copyAs<short>();
	data::TypedArray<uint8_t> byteArray = shortArray.copyAs<uint8_t>();
	data::enableLog<util::DefaultMsgPrint>( warning );

	for ( int i = 0; i < 12; i++ )
		BOOST_CHECK_EQUAL( shortArray[i], ceil( init[i] * 1e5 * scale - .5 ) );

	//with offset and scale
	const double uscale = std::numeric_limits< unsigned short >::max() / 4e5;
	data::enableLog<util::DefaultMsgPrint>( error );
	data::TypedArray<unsigned short> ushortArray = floatArray.copyAs<unsigned short>();
	data::enableLog<util::DefaultMsgPrint>( warning );


	for ( int i = 0; i < 12; i++ )
		BOOST_CHECK_EQUAL( ushortArray[i], ceil( init[i] * 1e5 * uscale + 32767.5 - .5 ) );
}

BOOST_AUTO_TEST_CASE( ValueArray_complex_minmax_test )
{
	const std::complex<float> init[] = { std::complex<float>( -2, 1 ), -1.8, -1.5, -1.3, -0.6, -0.2, 2, 1.8, 1.5, 1.3, 0.6, std::complex<float>( 10, 10 )};
	auto cfArray=data::ValueArray::make<std::complex<float> >(12 );
	cfArray.copyFromMem( init, 12 );

	auto minmax = cfArray.getMinMax();

	BOOST_CHECK_EQUAL( minmax.first.as<float >(),  0.2f );
	BOOST_CHECK_EQUAL( minmax.second.as<float >(), std::abs(std::complex<float>( 10, 10 )) );
}

BOOST_AUTO_TEST_CASE( ValueArray_complex_conversion_test )
{
	const std::complex<float> init[] = { -2, -1.8, -1.5, -1.3, -0.6, -0.2, 2, 1.8, 1.5, 1.3, 0.6, 0.2};
	data::TypedArray<std::complex<float>> cfArray= data::ValueArray::make<std::complex<float> >(12 );
	cfArray.copyFromMem( init, 12 );
	data::TypedArray<std::complex<double>> cdArray = cfArray.copyAs<std::complex<double> >();

	for( size_t i = 0; i < 12; i++ ) {
		BOOST_CHECK_EQUAL( cfArray[i], init[i] );
		BOOST_CHECK_EQUAL( cdArray[i], std::complex<double>( init[i] ) );
	}
}
BOOST_AUTO_TEST_CASE( ValueArray_numeric_to_complex_conversion_test )
{
	const float init[] = { -2, -1.8, -1.5, -1.3, -0.6, -0.2, 2, 1.8, 1.5, 1.3, 0.6, 0.2};
	auto fArray=data::ValueArray::make<float>(sizeof( init ) / sizeof( float ) );
	fArray.copyFromMem( init, sizeof( init ) / sizeof( float ) );

	data::TypedArray<std::complex<float> > cfArray = fArray.copyAs<std::complex<float> >();
	data::TypedArray<std::complex<double> > cdArray = fArray.copyAs<std::complex<double> >();

	// scalar values should be in real, imag should be 0
	for( size_t i = 0; i < sizeof( init ) / sizeof( float ); i++ ) {
		BOOST_CHECK_EQUAL( cfArray[i].real(), init[i] );
		BOOST_CHECK_EQUAL( cdArray[i].real(), init[i] );
		BOOST_CHECK_EQUAL( cfArray[i].imag(), 0 );
		BOOST_CHECK_EQUAL( cdArray[i].imag(), 0 );
	}
}


BOOST_AUTO_TEST_CASE( ValueArray_color_minmax_test )
{

	const util::color48 init[] = {
	    { 20, 180, 150},
	    {130,  60,  20},
	    { 20, 180, 150},
	    {130,  60,  20}
	};
	auto ccArray=data::ValueArray::make<util::color48>(4 );
	ccArray.copyFromMem( init, 4 );
	auto minmax = ccArray.getMinMax();
	const util::color48 colmin = {20, 60, 20}, colmax = {130, 180, 150};

	BOOST_CHECK_EQUAL( minmax.first.as<util::color48>(), colmin );
	BOOST_CHECK_EQUAL( minmax.second.as<util::color48>(), colmax );
}

BOOST_AUTO_TEST_CASE( ValueArray_color_conversion_test )
{
	const util::color48 init[] = { {0, 0, 0}, {100, 2, 4}, {200, 200, 200}, {510, 4, 2}};
	data::TypedArray<util::color48 > c16Array=data::ValueArray::make<util::color48 >(4 );
	c16Array.copyFromMem( init, 4 );

	data::TypedArray<util::color24 > c8Array = c16Array.copyAs<util::color24 >();// should downscale (by 2)

	for( size_t i = 0; i < 4; i++ ) {
		BOOST_REQUIRE_EQUAL( c16Array[i], init[i] );
		BOOST_CHECK_EQUAL( c8Array[i].r, init[i].r / 2 );
		BOOST_CHECK_EQUAL( c8Array[i].g, init[i].g / 2 );
		BOOST_CHECK_EQUAL( c8Array[i].b, init[i].b / 2 );
	}

	c16Array = c8Array.copyAs<util::color48>();//should not scale

	for( size_t i = 0; i < 4; i++ ) {
		BOOST_CHECK_EQUAL( c16Array[i], c8Array[i] );
	}

}


BOOST_AUTO_TEST_CASE( ValueArray_numeric_to_color_conversion_test )
{
	const short init[] = {0, 0, 0, 100, 2, 4, 200, 200, 200, 510, 4, 2};
	auto i16Array = data::ValueArray::make<uint16_t>(sizeof( init ) / sizeof( uint16_t ) );
	i16Array.copyFromMem( init, sizeof( init ) / sizeof( uint16_t ) );

	//scaling should be 0.5/0
	data::scaling_pair scale = i16Array.getScalingTo( util::typeID<util::color24 >() );
	BOOST_CHECK_EQUAL( scale.scale.as<double>(), 0.5 );
	BOOST_CHECK_EQUAL( scale.offset.as<double>(), 0 );

	data::TypedArray<util::color24 > c8Array = i16Array.copyAs<util::color24 >();// should downscale (by 2)

	for( size_t i = 0; i < sizeof( init ) / sizeof( uint16_t ); i++ ) {
		BOOST_CHECK_EQUAL( c8Array[i].r, init[i] / 2 );
		BOOST_CHECK_EQUAL( c8Array[i].g, init[i] / 2 );
		BOOST_CHECK_EQUAL( c8Array[i].b, init[i] / 2 );
	}

	data::TypedArray<util::color48 > c16Array = i16Array.copyAs<util::color48 >(); //should not scale

	for( size_t i = 0; i < sizeof( init ) / sizeof( uint16_t ); i++ ) {
		BOOST_CHECK_EQUAL( c16Array[i].r, init[i] );
		BOOST_CHECK_EQUAL( c16Array[i].g, init[i] );
		BOOST_CHECK_EQUAL( c16Array[i].b, init[i] );
	}
}

BOOST_AUTO_TEST_CASE( ValueArray_boolean_conversion_test )
{
	const float init[] = { -2, -1.8, -1.5, -1.3, -0.6, 0, 2, 1.8, 1.5, 1.3, 0.6, 0.2};
	auto cfArray = data::ValueArray::make<float>(12 );
	cfArray.copyFromMem( init, 12 );
	data::TypedArray<bool> bArray = cfArray.copyAs<bool>();
	data::TypedArray<float> fArray = bArray.copyAs<float>();

	for( size_t i = 0; i < 12; i++ ) {
		BOOST_CHECK_EQUAL( bArray[i], ( bool )init[i] );
		BOOST_CHECK_EQUAL( fArray[i], ( init[i] ? 1 : 0 ) );
	}
}

//BOOST_AUTO_TEST_CASE( ValueArray_minmax_test )
//{
//	const float init[] = {
//	    -std::numeric_limits<float>::infinity(),
//	    -1.8, -1.5, -1.3, -0.6, -0.2, 1.8, 1.5, 1.3,
//	    static_cast<float>( sqrt( -1 ) ),
//	    std::numeric_limits<float>::infinity(), 0.6, 0.2
//	};
//	auto floatArray = data::ValueArray::make<float>( sizeof( init ) / sizeof( float ) );
//	//without scaling
//	floatArray.copyFromMem( init, sizeof( init ) / sizeof( float ) );
//	{
//		std::pair<util::Value, util::Value> minmax = floatArray.getMinMax();
//		BOOST_CHECK( minmax.first.is<float>() );
//		BOOST_CHECK( minmax.second.is<float>() );
//		BOOST_CHECK_EQUAL( minmax.first.as<float>(), -1.8f );
//		BOOST_CHECK_EQUAL( minmax.second.as<float>(), 1.8f );
//	}
//}

template<typename T> void minMaxInt()
{
	data::TypedArray<T> array = data::ValueArray::make<T>(1024 );
	double div = static_cast<double>( RAND_MAX ) / std::numeric_limits<T>::max();

	for( int i = 0; i < 1024; i++ )
		array[i] = rand() / div;

	array[40] = std::numeric_limits<T>::max();
	array[42] = std::numeric_limits<T>::min();

	std::pair<util::Value, util::Value> minmax = array.getMinMax();
	BOOST_CHECK( minmax.first.is<T>() );
	BOOST_CHECK( minmax.second.is<T>() );
	BOOST_CHECK_EQUAL( minmax.first.as<T>(), std::numeric_limits<T>::min() );
	BOOST_CHECK_EQUAL( minmax.second.as<T>(), std::numeric_limits<T>::max() );
}
BOOST_AUTO_TEST_CASE( ValueArray_rnd_minmax_test )
{
	minMaxInt< uint8_t>();
	minMaxInt<uint16_t>();
	minMaxInt<uint32_t>();

	minMaxInt< int8_t>();
	minMaxInt<int16_t>();
	minMaxInt<int32_t>();

	minMaxInt< float>();
	minMaxInt<double>();
}


BOOST_AUTO_TEST_CASE( ValueArray_iterator_test )
{
	data::TypedArray<short> array = data::ValueArray::make<short>(1024 );

	for( int i = 0; i < 1024; i++ )
		array[i] = i + 1;

	size_t cnt = 0;

	for( auto i = array.begin(); i != array.end(); i++, cnt++ ) {
		BOOST_CHECK_EQUAL( *i, cnt + 1 ); // normal increment
		BOOST_CHECK_EQUAL( *( array.begin() + cnt ), cnt + 1 ); //+=
	}

	cnt=0;
	for( auto x: array ) {
		BOOST_CHECK_EQUAL( x, ++cnt ); // normal increment
	}


	BOOST_CHECK_EQUAL( *std::min_element( array.begin(), array.end() ), 1 );
	BOOST_CHECK_EQUAL( *std::max_element( array.begin(), array.end() ), *( array.end() - 1 ) );

	BOOST_CHECK_EQUAL( std::distance( array.begin(), array.end() ), 1024 );

	BOOST_CHECK_EQUAL( std::accumulate( array.begin(), array.end(), 0 ), 1024 * ( 1024 + 1 ) / 2 ); //gauss is my homie :-D

	std::fill( array.begin(), array.end(), 0 );
	BOOST_CHECK_EQUAL( std::accumulate( array.begin(), array.end(), 0 ), 0 );
}
BOOST_AUTO_TEST_CASE( ValueArray_generic_iterator_test )
{
	auto array = data::ValueArray::make<short>(1024 );

	// filling, using generic access
	for( int i = 0; i < 1024; i++ )
		array.begin()[i] = i + 1 ; //(miss)using the iterator for indexed access

	// check the content
	uint32_t cnt = 0;

	for( auto i = array.begin(); i != array.end(); i++, cnt++ ) {
		BOOST_CHECK_EQUAL( *i, cnt + 1 ); //this is using Value::eq
	}

	//check searching operations
	BOOST_CHECK_EQUAL( *std::min_element( array.begin(), array.end() ), *array.begin() );
	BOOST_CHECK_EQUAL( *std::max_element( array.begin(), array.end() ), *( array.end() - 1 ) );
	BOOST_CHECK( std::find( array.begin(), array.end(),5 ) == array.begin() + 4 ); //"1+4"

	BOOST_CHECK_EQUAL( std::distance( array.begin(), array.end() ), 1024 );
}
}
}
