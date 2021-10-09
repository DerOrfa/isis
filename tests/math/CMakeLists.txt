############################################################
# configure targets
############################################################

add_executable( fftTest fftTest.cpp )

target_link_libraries( fftTest isis_math Boost::unit_test_framework)

############################################################
# add ctest targets
############################################################

add_test(NAME fftTest COMMAND fftTest)
