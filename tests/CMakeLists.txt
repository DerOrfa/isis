############################################################
# The ISIS project
# 
# Main CMake configuration file of the ISIS tests.
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Thu, 07 Apr 2011 14:39:17 +0200
# 
############################################################

#makes and registers tests that need nothing but isis_core
macro( makeTest filename)
	get_filename_component(testname ${filename} NAME_WE)

	add_executable( ${testname} ${filename} )
	target_link_libraries( ${testname} isis_core Boost::unit_test_framework )
	add_test(NAME ${testname} COMMAND ${testname})
endmacro()

if(BUILD_TESTING)
	message(STATUS "Unit tests will be built (run with \"make test\")")

	if(USE_CONAN) #conan uses static boost son no need to check
		message(STATUS "Using conan. I'll assume boost is a static library / won't use \"-DBOOST_TEST_DYN_LINK\"")
	else(USE_CONAN)
		string(REGEX MATCH "\\.[aA]$" DOT_A ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
		string(REGEX MATCH "\\.[lL][iI][bB]$" DOT_LIB ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})

		if(DOT_LIB OR DOT_A)
			message(STATUS "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} seems to be a static library wont use \"-DBOOST_TEST_DYN_LINK\"")
		else()
			add_definitions("-DBOOST_TEST_DYN_LINK")
			message(STATUS "${Boost_UNIT_TEST_FRAMEWORK_LIBRARY} seems to be a dynamic library adding \"-DBOOST_TEST_DYN_LINK\"")
		endif()
	endif()

	add_subdirectory(util)
	add_subdirectory(data)
	add_subdirectory(io)
	add_subdirectory(math)
	add_subdirectory(adapter)

	if(ISIS_BUILD_FILTER )
		add_subdirectory(Filter)
	endif()

endif()

# addition option: build benchmarks
option(ISIS_BUILD_BENCHMARKS "Build the ISIS stress tests. For internal use only" OFF)

if(ISIS_BUILD_BENCHMARKS)
  message(STATUS "Building stress tests")
  add_subdirectory(benchmarks)
endif()
