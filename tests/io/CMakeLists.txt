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
makeTest(imageIOVistaTest.cpp)
