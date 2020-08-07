############################################################
# The ISIS project
# 
# Main CMake configuration file of the ISIS data tests.
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Thu, 07 Apr 2011 14:46:25 +0200
# 
############################################################

############################################################
# configure targets
############################################################

add_executable( chunkTest chunkTest.cpp )
add_executable( sortedchunklistTest sortedchunklistTest.cpp)
add_executable( imageTest imageTest.cpp )
add_executable( imageListTest imageListTest.cpp )
add_executable( valueArrayTest valueArrayTest.cpp )
add_executable( filePtrTest filePtrTest.cpp )
add_executable( byteswapTest byteswapTest.cpp )

target_link_libraries( valueArrayTest isis_core Boost::unit_test_framework )
target_link_libraries( filePtrTest isis_core Boost::unit_test_framework )
target_link_libraries( chunkTest isis_core Boost::unit_test_framework )
target_link_libraries( sortedchunklistTest isis_core Boost::unit_test_framework )
target_link_libraries( imageTest isis_math Boost::unit_test_framework )
target_link_libraries( imageListTest isis_core Boost::unit_test_framework )
target_link_libraries( byteswapTest isis_core Boost::unit_test_framework )

############################################################
# add unit test targets
############################################################

add_test(NAME chunkTest COMMAND chunkTest)
add_test(NAME sortedchunklistTest COMMAND sortedchunklistTest)
add_test(NAME imageTest COMMAND imageTest)
add_test(NAME imageListTest COMMAND imageListTest)
add_test(NAME valueArrayTest COMMAND valueArrayTest)
add_test(NAME filePtrTest COMMAND filePtrTest)
add_test(NAME byteswapTest COMMAND byteswapTest)
