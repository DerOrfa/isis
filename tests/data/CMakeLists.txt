makeTest( chunkTest.cpp )
makeTest( sortedchunklistTest sortedchunklistTest.cpp)
makeTest( imageListTest imageListTest.cpp )
makeTest( valueArrayTest valueArrayTest.cpp )
makeTest( filePtrTest filePtrTest.cpp )
makeTest( byteswapTest byteswapTest.cpp )

add_executable( imageTest imageTest.cpp )
target_link_libraries( imageTest isis_math Boost::unit_test_framework )
add_test(NAME imageTest COMMAND imageTest)

