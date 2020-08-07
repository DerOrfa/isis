set(ISIS_BUILD_STATIC_LIBS OFF CACHE BOOL "build all libraries static. If OFF a dynamic build will be done." )

############################################################
# macros to generate libs
############################################################
macro( build_lib name type source_glob header_glob header_infix deps private_deps soversion version)
	message(STATUS "${name} will be built as ${type} and installed to ${CMAKE_INSTALL_PREFIX}/lib")

	file(GLOB SRC_FILES ${source_glob})
	file(GLOB HDR_FILES ${header_glob})

	set(ext_deps ${deps})
	list(TRANSFORM ext_deps REPLACE "-TYPE" "-${type}")

	#build the library as -shared or as -static
	add_library( ${name}-${type} ${type} ${SRC_FILES} )
	target_link_libraries( ${name}-${type} PUBLIC ${ext_deps} PRIVATE ${private_deps})
	target_compile_features(${name}-${type} PUBLIC  cxx_std_17)
	set_target_properties( ${name}-${type} PROPERTIES 
		SOVERSION ${soversion} VERSION ${version} 
		OUTPUT_NAME ${name} INSTALL_NAME_DIR "${CMAKE_INSTALL_PREFIX}/lib"
		PUBLIC_HEADER "${HDR_FILES}"
		INTERFACE_POSITION_INDEPENDENT_CODE "ON" #theoreticaly for isis_core and the plugins only (but as all others depend on isis_core it doesn't matter)
	)

	# install libraries
	install (TARGETS ${name}-${type}
		RUNTIME DESTINATION bin COMPONENT RuntimeLibraries
		LIBRARY DESTINATION lib COMPONENT RuntimeLibraries
		ARCHIVE DESTINATION lib COMPONENT Development
		PUBLIC_HEADER DESTINATION "include/${header_infix}" COMPONENT Development
	)

	get_target_property(interface-deps  ${name}-${type} INTERFACE_LINK_LIBRARIES)
	list(LENGTH interface-deps num_of_interface-deps)
	if(${num_of_interface-deps})
		message(STATUS "Its external dependencies are:")
		foreach(_FILE ${interface-deps})
			message(STATUS "== ${_FILE}")
		endforeach()
	endif()

endmacro()

macro( add_lib name source_glob header_glob header_infix deps private_deps soversion version)
	if(ISIS_BUILD_STATIC_AND_SHARED)
		build_lib(${name} STATIC "${source_glob}" "${header_glob}" ${header_infix} "${deps}" "${private_deps}" ${soversion} ${version})
		build_lib(${name} SHARED "${source_glob}" "${header_glob}" ${header_infix} "${deps}" "${private_deps}" ${soversion} ${version})
		add_library(${name} ALIAS "${name}-SHARED")#use -SHARED as as alias for default link target internally
	else()
		if(ISIS_BUILD_STATIC_LIBS)
			build_lib(${name} STATIC "${source_glob}" "${header_glob}" ${header_infix} "${deps}" "${private_deps}" ${soversion} ${version})
			add_library(${name} ALIAS "${name}-STATIC")#use -STATIC as as alias for default link target internally
		else()
			build_lib(${name} SHARED "${source_glob}" "${header_glob}" ${header_infix} "${deps}" "${private_deps}" ${soversion} ${version})
			add_library(${name} ALIAS "${name}-SHARED")#use -SHARED as alias for default link target internally
		endif()
	endif()
endmacro()

############################################################
# Installation
############################################################

# install header files
install(FILES "config.hpp" DESTINATION include/isis COMPONENT Development)

add_subdirectory(core)
add_subdirectory(adapter)
add_subdirectory(math)

# write configuration file
configure_file(isis_cfg.cmake.in ${CMAKE_BINARY_DIR}/ISISConfig.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/ISISConfig.cmake DESTINATION share/isis/cmake COMPONENT Development)


