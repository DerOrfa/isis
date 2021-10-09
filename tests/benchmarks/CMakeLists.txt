############################################################
# The ISIS project
# 
# Main CMake configuration file of the ISIS data tests.
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Tue, 28 Jun 2011 18:46:55 +0200
# 
############################################################

############################################################
# configure targets
############################################################

add_executable( imageStresstest imageStresstest.cpp )
add_executable( valueArrayStresstest valueArrayStresstest.cpp )
add_executable( valueIteratorStresstest valueIteratorStresstest.cpp)
add_executable( typedIteratorStresstest typedIteratorStresstest.cpp)
add_executable( vectorStresstest vectorStresstest.cpp)
add_executable( chunkVoxelStressTest chunkVoxelStressTest.cpp )
add_executable( byteswapStressTest byteswapStresstest.cpp )
add_executable( swapDimStresstest swapDimStresstest.cpp )
add_executable( fftStresstest fftStresstest.cpp )

target_link_libraries( valueIteratorStresstest isis_core )
target_link_libraries( typedIteratorStresstest isis_core )
target_link_libraries( imageStresstest isis_core )
target_link_libraries( valueArrayStresstest isis_core )
target_link_libraries( vectorStresstest isis_core )
target_link_libraries( chunkVoxelStressTest isis_core )
target_link_libraries( byteswapStressTest isis_core )
target_link_libraries( swapDimStresstest isis_core )
target_link_libraries( fftStresstest isis_math )

############################################################
# add unit test targets
############################################################

# benchmarks are no default unit test targets
# add_test(NAME imageStresstest COMMAND imageStresstest)
# add_test(NAME typePtrStresstest COMMAND typePtrStresstest)

