############################################################
# The ISIS project
#
# Main CMake configuration file of the ISIS project.
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Tue, 05 Apr 2011 10:50:31 +0200
#
############################################################
cmake_minimum_required(VERSION 3.15.0)

############################################################
# set isis version number
############################################################
set(ISIS_VERSION_MAJOR "0")
set(ISIS_VERSION_MINOR "8")
set(ISIS_VERSION_PATCH "0")

set(ISIS_VERSION
  ${ISIS_VERSION_MAJOR}.${ISIS_VERSION_MINOR}.${ISIS_VERSION_PATCH}
  CACHE INTERNAL "The current Version of the ISIS project")

project(ISIS VERSION ${ISIS_VERSION})

find_package(Git)
find_package(Threads)

include(InstallRequiredSystemLibraries) #http://docdb.fnal.gov/ILC/sid/stanitz/ilcsoft-latest/cmake-2.4.8/Modules/InstallRequiredSystemLibraries.cmake
include(CheckIncludeFiles)
find_package(PkgConfig)


option(BUILD_TESTING OFF)
include(CTest)

add_definitions(-D_ISIS_VERSION_MAJOR=${ISIS_VERSION_MAJOR})
add_definitions(-D_ISIS_VERSION_MINOR=${ISIS_VERSION_MINOR})
add_definitions(-D_ISIS_VERSION_PATCH=${ISIS_VERSION_PATCH})


set(ISIS_CORE_VERSION_API "4.0.0")
string( REGEX MATCH "^[0-9]+" ISIS_CORE_VERSION_SO ${ISIS_CORE_VERSION_API})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED yes)

if(NOT WIN32)
############################################################
# set install infix for plugin installation path
############################################################
set(ISIS_PLUGIN_INFIX lib/isis/plugins)
endif()

############################################################
# set project specific module path
############################################################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

############################################################
# Logging options are global for all components in the tree
############################################################
option(ISIS_RUNTIME_LOG "Enable runtime logging" ON)
option(ISIS_DEBUG_LOG "Enable debug logging" ON)

############################################################
# optional components
############################################################
option(ISIS_BUILD_TOOLS "Build the ISIS command line tools (converter, diff, etc pp." OFF)

# enable RUNTIME_LOG
if(ISIS_RUNTIME_LOG)
  add_definitions(-D_ENABLE_LOG=1)
else()
  add_definitions(-D_ENABLE_LOG=0)
endif()

# enable DEBUG_LOG
if(ISIS_DEBUG_LOG)
  add_definitions(-D_ENABLE_DEBUG=1)
  message(STATUS "Debug code enabled, this will will have a big impact on the performance")
else()
  add_definitions(-D_ENABLE_DEBUG=0)
endif()

# since ISIS stongly depends on the boost libraries we will configure them
# globally.
if(BUILD_TESTING)
	find_package(Boost REQUIRED COMPONENTS iostreams unit_test_framework)
else()
	find_package(Boost REQUIRED COMPONENTS iostreams)
endif()

############################################################
# Macro to generate manpages for CLI programms
############################################################
set(MANPAGES OFF CACHE BOOL "Create manpages for the tools")

if(MANPAGES)
	find_program(HELP2MAN help2man)
endif()

macro( build_manpage name desc)
	if(MANPAGES)
		add_custom_command(
			TARGET ${name}
			POST_BUILD
			COMMAND ${HELP2MAN} ./${name} --version-string=${ISIS_CORE_REV_STRING} --no-discard-stderr --no-info -o ${name}.1 -s 1 --name=${desc}
			COMMENT "(Re)Generating manpage ${name}.1"
            VERBATIM
		)
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${name}.1 DESTINATION share/man/man1 COMPONENT "CLI_Tools")
	endif()
endmacro()

############################################################
# RCS revision number
############################################################
# The revision number is based on GIT commit information.
# It will be stored in GIT_REVISION.
if (NOT GIT_REVISION)
  if (GIT_FOUND)
    execute_process(COMMAND Git::Git describe OUTPUT_VARIABLE OUT WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCH "[0-9]+-[^-]+$" GIT_REVISION "${OUT}") #@todo wont work with more complex revisions/modified sources
    set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}-${GIT_REVISION}-${CMAKE_SYSTEM_NAME}-${PROCESSOR}-${CMAKE_BUILD_TYPE})
  else()
    message(STATUS "git not found")
  endif ()
endif ()

if (GIT_REVISION)
  message(STATUS "RCS Revision: ${GIT_REVISION}")
  add_definitions(-DISIS_RCS_REVISION=${GIT_REVISION})
  set(ISIS_CORE_REV_STRING ${ISIS_VERSION_MAJOR}.${ISIS_VERSION_MINOR}.${ISIS_VERSION_PATCH}-${GIT_REVISION})
else()
  set(ISIS_CORE_REV_STRING ${ISIS_VERSION_MAJOR}.${ISIS_VERSION_MINOR}.${ISIS_VERSION_PATCH})
endif()

###########################################################
# CPack config
###########################################################
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "isis core library")
set(CPACK_PACKAGE_VENDOR "the friendly isis team")
set(CPACK_PACKAGE_CONTACT "reimer@cbs.mpg.de")
set(CPACK_PACKAGE_NAME "isis-core")
set(CPACK_BUNDLE_NAME ${CPACK_PACKAGE_NAME})
set(CPACK_COMPONENTS_ALL "CLI_Tools" "IO_plugins" "Development" "RuntimeLibraries")
#SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ReadMe.txt")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING.txt")
set(CPACK_PACKAGE_VERSION_MAJOR ${ISIS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${ISIS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${ISIS_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME ${CPACK_PACKAGE_NAME}-${ISIS_CORE_REV_STRING}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE})
SET(CPACK_PACKAGING_INSTALL_PREFIX "/usr") #prevent the productbuild generator from stuffing files into "/Applications"
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_NSIS_URL_INFO_ABOUT "http:\\\\\\\\isis-group.github.com/isis")
include(CPack)

###########################################################
# Doxygen target
###########################################################
option(BUILD_DOCUMENTATION "Create and install the HTML based API documentation (requires Doxygen)" false)
if(BUILD_DOCUMENTATION)
	find_package(Doxygen)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen and latex are needed to build the documentation.")
    endif()

    set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Doxyfile.in)
    set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${doxyfile_in} ${doxyfile} @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)

#    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION share/doc)
endif()




# build the ISIS libraries
add_subdirectory(isis)

# configure tests
add_subdirectory(tests)

# build the ISIS tools.
if(ISIS_BUILD_TOOLS)
  add_subdirectory(tools)
endif()

add_subdirectory(io_plugins)

# uninstall target
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake//cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
