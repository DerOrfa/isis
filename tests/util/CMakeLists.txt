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

makeTest( commonTest.cpp )
makeTest( propertyTest.cpp )
makeTest( valueTest.cpp )
makeTest( vectorTest.cpp )
makeTest( propMapTest.cpp )
makeTest( singletonTest.cpp )
makeTest( selectionTest.cpp )
makeTest( istringTest.cpp )
