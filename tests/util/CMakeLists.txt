############################################################
# The ISIS project
# 
# Main CMake configuration file of the ISIS util tests.
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Thu, 07 Apr 2011 14:41:35 +0200
# 
############################################################

############################################################
# configure targets
############################################################

add_executable( propertyTest propertyTest.cpp )
add_executable( valueTest valueTest.cpp )
add_executable( vectorTest vectorTest.cpp )
add_executable( propMapTest propMapTest.cpp )
add_executable( singletonTest singletonTest.cpp )
add_executable( selectionTest selectionTest.cpp )
add_executable( commonTest commonTest.cpp )
add_executable( istringTest istringTest.cpp )

target_link_libraries( commonTest isis_core Boost::unit_test_framework )
target_link_libraries( propertyTest isis_core Boost::unit_test_framework  )
target_link_libraries( valueTest isis_core Boost::unit_test_framework)
target_link_libraries( vectorTest isis_core Boost::unit_test_framework)
target_link_libraries( propMapTest isis_core Boost::unit_test_framework)
target_link_libraries( singletonTest isis_core Boost::unit_test_framework)
target_link_libraries( selectionTest isis_core Boost::unit_test_framework)
target_link_libraries( istringTest isis_core Boost::unit_test_framework)

############################################################
# add ctest targets
############################################################

add_test(NAME propertyTest COMMAND propertyTest)
add_test(NAME commonTest COMMAND commonTest)
add_test(NAME valueTest COMMAND valueTest)
add_test(NAME vectorTest COMMAND vectorTest)
add_test(NAME propMapTest COMMAND propMapTest)
add_test(NAME singletonTest COMMAND singletonTest)
add_test(NAME selectionTest COMMAND selectionTest)
add_test(NAME istringTest COMMAND istringTest)
