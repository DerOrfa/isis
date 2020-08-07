############################################################
# The ISIS project
# 
# Main CMake configuration file of the ISIS ImageIO tests.
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Thu, 07 Apr 2011 14:48:25 +0200
# 
############################################################

############################################################
# configure targets
############################################################

add_executable(imageIONullTest imageIONullTest.cpp)
add_executable(imageIONiiTest imageIONiiTest.cpp)
add_executable(imageIOTest imageIOTest.cpp)
add_executable(imageIOVistaTest imageIOVistaTest.cpp)

target_link_libraries(imageIONullTest  isis_core Boost::unit_test_framework)
target_link_libraries(imageIONiiTest   isis_core Boost::unit_test_framework)
target_link_libraries(imageIOVistaTest isis_core Boost::unit_test_framework)
target_link_libraries(imageIOTest      isis_core Boost::unit_test_framework)
target_link_libraries(imageIOVistaTest isis_core Boost::unit_test_framework)
