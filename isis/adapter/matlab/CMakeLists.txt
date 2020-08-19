macro( build_mex name source_glob private_deps)
	message(STATUS "matlab module ${name} will be installed to ${CMAKE_INSTALL_FULL_LIBDIR}")

	file(GLOB SRC_FILES ${source_glob})

	matlab_add_mex(NAME ${name} MODULE SRC ${SRC_FILES} common.cpp property_transfer.cpp image_transfer.cpp LINK_TO ${private_deps} R2018a)
	target_compile_features(${name} PUBLIC cxx_std_17)

	# install libraries
	install (TARGETS ${name}
		EXPORT isis-exports
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT RuntimeLibraries
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT RuntimeLibraries
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development
	)
endmacro()

find_package(Matlab REQUIRED COMPONENTS MEX_COMPILER)

build_mex(isisload load.cpp isis_core)
