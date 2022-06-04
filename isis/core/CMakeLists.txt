############################################################
# find jsoncpp
############################################################
find_library(LIB_JSONCPP jsoncpp)
find_path(INC_JSONCPP "json/config.h" PATH_SUFFIXES "jsoncpp" DOC "path where json/config.h can be found")
include_directories(${INC_JSONCPP})

############################################################
# check for ncurses
############################################################
find_package(Curses QUIET)
if(CURSES_LIBRARIES)
	include_directories(${CURSES_CURSES_H_PATH})
	message(STATUS "curses form libraries ${CURSES_LIBRARIES} found. Enable colored logging.")
else()
	message(STATUS "curses form library not found. Logging won't be colored.")
	set(CURSES_LIBRARIES "")
endif()

############################################################
# check for liborc
############################################################
set(ISIS_USE_LIBORC OFF CACHE BOOL "use the \"Oil Runtime Compiler\"" )
if(ISIS_USE_LIBORC)
	find_file(LIBORC_H "orc/orc.h" DOC "Where to find the orc include files (looks for orc/orc.h)" )
	find_library(LIBORC_LIB "orc-0.4")

	find_program(LIBORC_COMPILER "orcc")

	add_custom_command(
		COMMAND ${LIBORC_COMPILER} ARGS --implementation ${CMAKE_CURRENT_SOURCE_DIR}/type_conversions.orc -o type_conversions.c
		COMMAND ${LIBORC_COMPILER} ARGS --header ${CMAKE_CURRENT_SOURCE_DIR}/type_conversions.orc -o type_conversions.h
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/type_conversions.orc
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		OUTPUT ${CMAKE_BINARY_DIR}/type_conversions.c
	)

    include_directories(${LIBORC_H})
	include_directories(${CMAKE_BINARY_DIR})#so we can #include the generated orc-code
	list(APPEND CORE_SRC_FILES ${CMAKE_BINARY_DIR}/type_conversions.c)
else()
	add_definitions(-DDISABLE_ORC)
	set(LIBORC_LIB "")
endif()

#see also https://stackoverflow.com/questions/41676311
set_source_files_properties( "valuearray_minmax.cpp" PROPERTIES COMPILE_FLAGS "-Wno-ignored-attributes")

if(CURSES_LIBRARIES)
set_source_files_properties( "message.cpp" PROPERTIES COMPILE_FLAGS "-DHAVE_CURSES")
endif()

#add the core library
add_lib(isis_core
	"*.cpp" "*.hpp;*.h" "isis/core"
	"boost::headers" #deps
	"${LIBORC_LIB};${CURSES_LIBRARIES};${CMAKE_DL_LIBS};${LIB_JSONCPP};Threads::Threads;$<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>" #private deps
	${ISIS_CORE_VERSION_SO} ${ISIS_CORE_VERSION_API}
)

############################################################
# set the plugin directory and the system dependent shared
# library naming conventions for data/io_factory.cpp
############################################################
if(WIN32)
set_source_files_properties( "io_factory.cpp" PROPERTIES COMPILE_FLAGS
	"-DDL_SUFFIX=\\\"${CMAKE_SHARED_LIBRARY_SUFFIX}\\\" -DDL_PREFIX=\\\"${CMAKE_SHARED_LIBRARY_PREFIX}\\\""
)
else()
set_source_files_properties( "io_factory.cpp" PROPERTIES COMPILE_FLAGS
	"-DPLUGIN_PATH=\\\"${CMAKE_INSTALL_PREFIX}/${ISIS_PLUGIN_INFIX}\\\" -DDL_SUFFIX=\\\"${CMAKE_SHARED_LIBRARY_SUFFIX}\\\" -DDL_PREFIX=\\\"${CMAKE_SHARED_LIBRARY_PREFIX}\\\""
)
endif()
