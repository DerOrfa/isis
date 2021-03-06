#include <isis/core/endianess.hpp>
#include <isis/core/valuearray.hpp>
#include <boost/timer.hpp>
#include <isis/core/valuearray_typed.hpp>

using namespace isis;

template<typename T> void testEndianSwap( size_t size )
{
	const data::TypedArray<T> source( size );
	data::TypedArray<T> target( size );

	boost::timer timer;
	data::endianSwapArray( source.begin(), source.end(), target.begin() );

	std::cout
			<< "byteswapped " << size << " elements " << util::typeName<T>()
			<< " in " << timer.elapsed() << " seconds " << std::endl;

}
int main()
{
	testEndianSwap<uint8_t>( 1024 * 1024 * 1024 / sizeof( uint8_t ) );
	testEndianSwap<uint16_t>( 1024 * 1024 * 1024 / sizeof( uint16_t ) );
	testEndianSwap<uint32_t>( 1024 * 1024 * 1024 / sizeof( uint32_t ) );
	testEndianSwap<float>( 1024 * 1024 * 1024 / sizeof( float ) );
	testEndianSwap<double>( 1024 * 1024 * 1024 / sizeof( double ) );
	testEndianSwap<util::color48>( 1024 * 1024 * 1024 / sizeof( util::color48 ) );
	return 0;
}
