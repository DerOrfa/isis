set(ISIS_BUILD_STATIC_LIBS OFF CACHE BOOL "build all libraries static. If OFF a dynamic build will be done." )


############################################################
# macros to generate libs
############################################################
macro( build_lib name type sources deps soversion version)
	message(STATUS "${name} will be built as ${type} and installed to ${CMAKE_INSTALL_PREFIX}/lib")

	#build the library as -shared or as -static
	add_library( ${name}-${type} ${type} ${sources} )
	target_link_libraries( ${name}-${type} ${deps})
	set_target_properties( ${name}-${type} PROPERTIES SOVERSION ${soversion} VERSION ${version} OUTPUT_NAME ${name} INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib")

	# install libraries
	install (TARGETS ${name}-${type}
		RUNTIME DESTINATION bin COMPONENT RuntimeLibraries
		LIBRARY DESTINATION lib COMPONENT RuntimeLibraries
		ARCHIVE DESTINATION lib COMPONENT Development
	)

	set(${name}_lib ${name}-${type} CACHE INTERNAL "internal name for ${name}" FORCE)
endmacro( build_lib )

macro( add_lib name sources deps soversion version)
	if(ISIS_BUILD_STATIC_AND_SHARED)
		build_lib(${name} STATIC "${sources}" "${deps}" ${soversion} ${version})
		build_lib(${name} SHARED "${sources}" "${deps}" ${soversion} ${version}) #this will cause the shared libs logic name  to be used in ${name}_lib
	else(ISIS_BUILD_STATIC_AND_SHARED)
		if(ISIS_BUILD_STATIC_LIBS)
			build_lib(${name} STATIC "${sources}" "${deps}" ${soversion} ${version})
		else(ISIS_BUILD_STATIC_LIBS)
			build_lib(${name} SHARED "${sources}" "${deps}" ${soversion} ${version})
		endif(ISIS_BUILD_STATIC_LIBS)
	endif(ISIS_BUILD_STATIC_AND_SHARED)
endmacro( add_lib )

find_library(LIB_JSONCPP jsoncpp)
find_path(INC_JSONCPP "json/version.h" PATH_SUFFIXES "jsoncpp" DOC "path where json/version.h can be found")
include_directories(${INC_JSONCPP})

############################################################
# check for liboil
############################################################
set(ISIS_USE_LIBOIL OFF CACHE BOOL "use liboil" )
if(ISIS_USE_LIBOIL)
	find_path(LibOil_INCLUDE_DIR "liboil/liboil.h" PATH_SUFFIXES "liboil" "liboil-0.3")
	find_library(LibOil_LIBRARY NAMES "oil-0.3")
	include_directories(${LibOil_INCLUDE_DIR})
	add_definitions(-DISIS_USE_LIBOIL)
else(ISIS_USE_LIBOIL)
	set(LibOil_LIBRARY "")
endif(ISIS_USE_LIBOIL)

############################################################
# export library dependencies of isis_core
############################################################
foreach(FILE ${CMAKE_DL_LIBS} ${Boost_LIBRARIES} ${LibOil_LIBRARY} ${LIB_JSONCPP})
		string(REGEX MATCH "${CMAKE_SHARED_LIBRARY_SUFFIX}$" IS_SHARED ${FILE})
		if(IS_SHARED)
				LIST(APPEND ISIS_LIB_DEPENDS ${FILE})
		endif(IS_SHARED)
endforeach(FILE)
if(ISIS_LIB_DEPENDS)
	message(STATUS "${CMAKE_SYSTEM_NAME} core dependencies are:  ${ISIS_LIB_DEPENDS}")
else(ISIS_LIB_DEPENDS)
	message(STATUS "no core dependencies detected")
endif(ISIS_LIB_DEPENDS)


############################################################
# build the core library
############################################################

# add source files to this parameter list
file(GLOB CORE_SRC_FILES "data/*.cpp" "util/*.cpp")
file(GLOB COREUTILS_HDR_FILES "util/*.hpp" "util/*.h")
file(GLOB DATASTORAGE_HDR_FILES "data/*.hpp" "data/*.h")

#add the core library
add_lib(isis_core
	"${CORE_SRC_FILES}"
	"${CMAKE_DL_LIBS};${LibOil_LIBRARY};${Boost_LIBRARIES};${LIB_JSONCPP}"
	${ISIS_CORE_VERSION_SO} ${ISIS_CORE_VERSION_API}
)

############################################################
# set the plugin directory and the system dependend shared
# library naming conventions for data/io_factory.cpp
############################################################
if(WIN32)
set_source_files_properties( "data/io_factory.cpp" PROPERTIES COMPILE_FLAGS
	"-DDL_SUFFIX=\\\"${CMAKE_SHARED_LIBRARY_SUFFIX}\\\" -DDL_PREFIX=\\\"${CMAKE_SHARED_LIBRARY_PREFIX}\\\""
)
else(WIN32)
set_source_files_properties( "data/io_factory.cpp" PROPERTIES COMPILE_FLAGS
	"-DPLUGIN_PATH=\\\"${CMAKE_INSTALL_PREFIX}/${ISIS_PLUGIN_INFIX}\\\" -DDL_SUFFIX=\\\"${CMAKE_SHARED_LIBRARY_SUFFIX}\\\" -DDL_PREFIX=\\\"${CMAKE_SHARED_LIBRARY_PREFIX}\\\""
)
endif(WIN32)

############################################################
# Installation
############################################################

# install header files
install(FILES "config.hpp" DESTINATION include/isis COMPONENT Development)
install(FILES ${COREUTILS_HDR_FILES} DESTINATION include/isis/util COMPONENT Development)
install(FILES ${DATASTORAGE_HDR_FILES} DESTINATION include/isis/data COMPONENT Development)

# write configuration file
configure_file(cmake/isis_corecfg.cmake.in ${CMAKE_BINARY_DIR}/ISISConfig.cmake @ONLY)

# install cmake configuration files
install(FILES ${CMAKE_BINARY_DIR}/ISISConfig.cmake DESTINATION share/isis/cmake COMPONENT Development)

add_subdirectory(adapter)
add_subdirectory(math)
