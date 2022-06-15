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

makeTest(imageIONullTest.cpp)
makeTest(imageIONiiTest.cpp)
makeTest(imageIOTest.cpp)

if(ISIS_IOPLUGIN_VISTA_SA)
add_executable( imageIOVistaTest imageIOVistaTest.cpp )
target_link_libraries( imageIOVistaTest isis_math Boost::unit_test_framework )
add_test(NAME imageIOVistaTest COMMAND imageIOVistaTest)
endif(ISIS_IOPLUGIN_VISTA_SA)
