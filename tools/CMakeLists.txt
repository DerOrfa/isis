###########################################################
# find and configure the isis core library
###########################################################
set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR})

############################################################
# configure the targets
############################################################
add_executable(isisdump isisdump.cpp)
target_link_libraries(isisdump isis_core)
set_target_properties(isisdump PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
build_manpage(isisdump "print out the metadata of an image")

add_executable(isisdiff isisdiff.cpp)
target_link_libraries(isisdiff isis_core)
set_target_properties(isisdiff PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
build_manpage(isisdiff "get a \"diff\" of the metadata of two images")

add_executable(isisconv isisconv.cpp)
target_link_libraries(isisconv isis_core)
set_target_properties(isisconv PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
build_manpage(isisconv "general MR image converter")

add_executable(isisflip isisflip.cpp)
target_link_libraries(isisflip isis_math)
set_target_properties(isisflip PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
build_manpage(isisflip "flip MR images along specific dimensions")

add_executable(isisraw isisraw.cpp)
target_link_libraries(isisraw isis_core)
set_target_properties(isisraw PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
build_manpage(isisraw "read or write raw data files from/to isis images")

add_executable(isisunwrap isisunwrap.cpp)
target_link_libraries(isisunwrap isis_core)
set_target_properties(isisunwrap PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
build_manpage(isisunwrap "read or write raw data files from/to isis images")

# the install targets.
install(TARGETS
  isisdump
  isisdiff
  isisconv
  isisflip
  isisunwrap
  isisraw RUNTIME DESTINATION bin COMPONENT "CLI_Tools")

if(ISIS_QT5)
	find_package(Qt5 COMPONENTS Widgets)
	add_executable(isisview isisview.cpp)
	target_link_libraries(isisview isis_qt5 )
	set_target_properties(isisview PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)

	install(TARGETS isisview RUNTIME DESTINATION bin COMPONENT "CLI_Tools")
	build_manpage(isisview "display MRI data")
endif()

if(ISIS_ITK)
	add_executable(isistransform isistransform.cpp)
	target_link_libraries(isistransform isis_math isis_itk4 )
	set_target_properties(isistransform PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
	
	install(TARGETS isistransform RUNTIME DESTINATION bin COMPONENT "CLI_Tools")
	build_manpage(isistransform "geometricaly transform MR images")
endif()

############################################################
# optional components
############################################################

option(ISIS_CALC "Build the ISIS image calculator (needs the muparser library)" OFF)

# isiscalc
if(ISIS_CALC)
	find_library(LIB_MUPARSER "muparser" REQUIRED)
	find_path(INCPATH_MUPARSER "muParser.h" PATH_SUFFIXES "muParser")
	
	add_executable(isiscalc isiscalc.cpp)
	target_link_libraries(isiscalc ${LIB_MUPARSER} isis_core)
	set_target_properties(isiscalc PROPERTIES 
		INSTALL_RPATH_USE_LINK_PATH TRUE
		INCLUDE_DIRECTORIES "${INCPATH_MUPARSER};${CMAKE_SOURCE_DIR}"
	)

	install(TARGETS isiscalc RUNTIME DESTINATION bin COMPONENT "CLI Tools")
	build_manpage(isiscalc "do math on image voxels (based on muparser)")
endif()

