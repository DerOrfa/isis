############################################################
# The ISIS project
# 
# CMake configuration file of the ISIS I/O plugins
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Wed, 06 Apr 2011 19:23:10 +0200
# 
############################################################

############################################################
# include the ISIS core header files
############################################################
set_directory_properties(PROPERTIES INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR})

############################################################
# add global options
############################################################
option(ISIS_IOPLUGIN_NIFTI_SA "Enable standalone Nifti-IO plugin" ON)
option(ISIS_IOPLUGIN_TIFF_SA "Enable standalone TIFF-IO plugin" OFF)
option(ISIS_IOPLUGIN_ZISRAW "Enable standalone Zeiss Integrated Software RAW plugin" OFF)
option(ISIS_IOPLUGIN_ORTHANC "Enable standalone plugin for reading siemens RAW data files" OFF)
option(ISIS_IOPLUGIN_VISTA_SA "Enable standalone Vista-IO plugin" OFF)
option(ISIS_IOPLUGIN_NULL "Enable Null-IO plugin" OFF)
option(ISIS_IOPLUGIN_PNG "Enable PNG-IO plugin" OFF)
option(ISIS_IOPLUGIN_IMAGEMAGIC "Enable plugin for ImageMagic based IO" OFF)
option(ISIS_IOPLUGIN_DICOM "Enable Dicom-IO plugin" ON)
option(ISIS_IOPLUGIN_COMP "Enable proxy plugin for compressed data (gz, bzip2 and Z)" ON)
option(ISIS_IOPLUGIN_TAR "Enable proxy plugin for reading tar archives" ON)
option(ISIS_IOPLUGIN_FDF "Enable plugin for reading fdf files" OFF)
option(ISIS_IOPLUGIN_FLIST "Enable proxy plugin which gets filenames from a file or stdin" ON)
option(ISIS_IOPLUGIN_SFTP "Enable proxy plugin for sftp" ON)

############################################################
# the plugins ...
############################################################

############################################################
# fdf plugin
############################################################
if(ISIS_IOPLUGIN_FDF)
	add_library(isisImageFormat_fdf SHARED imageFormat_fdf.cpp )
	target_link_libraries(isisImageFormat_fdf isis_core)
	set(TARGETS ${TARGETS} isisImageFormat_fdf)
endif()

############################################################
# NIFTI plugin
############################################################
if(ISIS_IOPLUGIN_NIFTI_SA)
	add_library(isisImageFormat_Nifti_sa SHARED imageFormat_nifti_sa.cpp imageFormat_nifti_dcmstack.cpp )
	target_link_libraries(isisImageFormat_Nifti_sa isis_math)
	set(TARGETS ${TARGETS} isisImageFormat_Nifti_sa)
endif()

############################################################
# orthanc plugin
############################################################
if(ISIS_IOPLUGIN_ORTHANC)
	add_library(isisImageFormat_orthanc SHARED imageFormat_orthanc.cpp )
	set_target_properties(isisImageFormat_orthanc PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR};${INC_JSONCPP}")
	target_link_libraries(isisImageFormat_orthanc isis_core ACE ACE_INet jsoncpp Threads::Threads)
	set(TARGETS ${TARGETS} isisImageFormat_orthanc)
endif()

############################################################
# TIFF plugin
############################################################
if(ISIS_IOPLUGIN_TIFF_SA)
	find_path(TJPEG_INCLUDE_DIR "turbojpeg.h")
	find_library(TJPEG_LIBRARY "turbojpeg")

	add_library(isisImageFormat_tiff_sa SHARED imageFormat_tiff_sa.cpp )
	set_target_properties(isisImageFormat_tiff_sa PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR};${TJPEG_INCLUDE_DIR}")
	target_link_libraries(isisImageFormat_tiff_sa ${TJPEG_LIBRARY} isis_core)
	set(TARGETS ${TARGETS} isisImageFormat_tiff_sa)
endif()

if(ISIS_IOPLUGIN_ZISRAW)
	find_path(JXR_INCLUDE_DIR "JXRGlue.h" PATH_SUFFIXES "jxrlib" DOC "libjxr include path")

	find_path(JXR_WINDOWSMEDIAPHOTO_DIR "windowsmediaphoto.h" PATH_SUFFIXES "image/sys" HINTS ${JXR_INCLUDE_DIR} DOC "path where to find windowsmediaphoto.h")
	find_path(JXR_WMSPECSTRING_DIR "wmspecstring.h" PATH_SUFFIXES "common/include" HINTS ${JXR_INCLUDE_DIR} DOC "path where to find wmspecstring.h")

	find_library(JXR_GLUE_LIBRARY "jxrglue")
	find_library(JXR_LIBRARY "jpegxr")
	
	add_library(isisImageFormat_ZISRAW SHARED imageFormat_ZISRAW.cpp imageFormat_ZISRAW_jxr.c)
	set_target_properties(isisImageFormat_ZISRAW PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR};${JXR_INCLUDE_DIR};${JXR_WINDOWSMEDIAPHOTO_DIR};${JXR_WMSPECSTRING_DIR}")
	
	target_link_libraries(isisImageFormat_ZISRAW isis_core ${JXR_GLUE_LIBRARY} ${JXR_LIBRARY} Threads::Threads)
	set(TARGETS ${TARGETS} isisImageFormat_ZISRAW)
endif()

if(ISIS_IOPLUGIN_VISTA_SA)
	option(REGEN_PARSER "Regenerate the parser files" OFF)
	if(REGEN_PARSER)
		find_program(FLEXc++_EXECUTABLE "flexc++")
		find_program(BISONc++_EXECUTABLE "bisonc++")

		ADD_CUSTOM_COMMAND(
			COMMAND ${FLEXc++_EXECUTABLE} 
			ARGS --target-directory=${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/ ${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/lexer.l
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/lexer.l
			OUTPUT 
				${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/lex.cc 
				${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/VistaScanner.ih
				${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/VistaScannerbase.h
		)

		ADD_CUSTOM_COMMAND(
			COMMAND ${BISONc++_EXECUTABLE} 
			ARGS --target-directory=${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/ ${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/grammar.y
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/lex.cc ${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/grammar.y
			OUTPUT 
				${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/parse.cc
				${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/VistaParser.ih
				${CMAKE_CURRENT_SOURCE_DIR}/Vista_sa/VistaParserbase.h
		)
	endif(REGEN_PARSER)

	add_library(isisImageFormat_Vista_sa SHARED 
		Vista_sa/imageFormat_Vista_sa.cpp Vista_sa/vistaprotoimage.cpp 
		Vista_sa/lex.cc Vista_sa/parse.cc
	)
	target_link_libraries(isisImageFormat_Vista_sa isis_core)
	set(TARGETS ${TARGETS} isisImageFormat_Vista_sa)
endif()


############################################################
# NULL plugin
############################################################
if(ISIS_IOPLUGIN_NULL)
  add_library(isisImageFormat_Null SHARED imageFormat_Null.cpp)
  target_link_libraries(isisImageFormat_Null isis_core)
  set(TARGETS ${TARGETS} isisImageFormat_Null)
endif()

############################################################
# PNG plugin
############################################################
if(ISIS_IOPLUGIN_PNG)
	find_package(PNG REQUIRED)

	add_library(isisImageFormat_png SHARED imageFormat_png.cpp)
	target_link_libraries(isisImageFormat_png isis_core PNG::PNG Threads::Threads)

	set(TARGETS ${TARGETS} isisImageFormat_png)
endif()

############################################################
# ImageMagic plugin
############################################################
if(ISIS_IOPLUGIN_IMAGEMAGIC)
	find_package(ImageMagick REQUIRED Magick++ MagickWand MagickCore)

	add_library(isisImageFormat_ImageMagick SHARED imageFormat_ImageMagick.cpp)
	set_target_properties(isisImageFormat_ImageMagick PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR};${ImageMagick_INCLUDE_DIRS}")
	target_link_libraries(isisImageFormat_ImageMagick isis_core ${ImageMagick_LIBRARIES})

	add_definitions("-DMAGICKCORE_HDRI_ENABLE=0" "-DMAGICKCORE_QUANTUM_DEPTH=16")

	set(TARGETS ${TARGETS} isisImageFormat_ImageMagick)
endif()

############################################################
# COMP proxy plugin
############################################################
if(ISIS_IOPLUGIN_COMP)
	add_library(isisImageFormat_comp_proxy SHARED imageFormat_compressed.cpp)
	target_link_libraries(isisImageFormat_comp_proxy isis_core Boost::iostreams)
	set(TARGETS ${TARGETS} isisImageFormat_comp_proxy)
endif()

############################################################
# TAR proxy plugin
############################################################
if(ISIS_IOPLUGIN_TAR)
  add_library(isisImageFormat_tar_proxy SHARED imageFormat_tar.cpp)
  target_link_libraries(isisImageFormat_tar_proxy isis_core Boost::iostreams)
  set(TARGETS ${TARGETS} isisImageFormat_tar_proxy)
endif()

############################################################
# FLIST proxy plugin
############################################################
if(ISIS_IOPLUGIN_FLIST)
	add_library(isisImageFormat_flist SHARED imageFormat_flist.cpp)
	target_link_libraries(isisImageFormat_flist isis_core)
	set(TARGETS ${TARGETS} isisImageFormat_flist)
endif()

############################################################
# Dicom
############################################################
if(ISIS_IOPLUGIN_DICOM)
	find_library(OPENJPEG_LIBRARY openjp2)
	find_path(OPENJPEG_INCLUDE_DIR openjpeg.h PATH_SUFFIXES openjpeg-2.3 openjpeg-2.4)

	add_library(isisImageFormat_Dicom SHARED dicom/imageFormat_Dicom.cpp dicom/imageFormat_DicomParser.cpp dicom/imageFormat_DicomDictionary.cpp)
	
	if(OPENJPEG_INCLUDE_DIR AND OPENJPEG_INCLUDE_DIR)
		target_compile_definitions(isisImageFormat_Dicom PRIVATE "HAVE_OPENJPEG")
		target_sources(isisImageFormat_Dicom PRIVATE dicom/imageFormat_DicomJ2k.cpp)
		target_include_directories(isisImageFormat_Dicom PRIVATE ${OPENJPEG_INCLUDE_DIR})
		target_link_libraries(isisImageFormat_Dicom PRIVATE isis_core ${OPENJPEG_LIBRARY})
	else(OPENJPEG_INCLUDE_DIR AND OPENJPEG_INCLUDE_DIR)
		message(WARNING "OpenJPEG not found, support for j2k-streams in dicom import (transfer syntax 1.2.840.10008.1.2.4.90) disabled")
		target_link_libraries(isisImageFormat_Dicom PRIVATE isis_core)
	endif(OPENJPEG_INCLUDE_DIR AND OPENJPEG_INCLUDE_DIR)
	set(TARGETS ${TARGETS} isisImageFormat_Dicom)
endif()

if(ISIS_IOPLUGIN_SFTP)
	find_package(Libssh2 QUIET)
	add_library(isisImageFormat_sftp SHARED sftp/imageFormat_sftp.cpp sftp/sftpclient.cpp)

	if(TARGET Libssh2::libssh2)#if find_package(Libssh2) above was successful
		target_link_libraries(isisImageFormat_sftp isis_core Libssh2::libssh2)
	else(TARGET Libssh2::libssh2)#ok find_package didn't get us a target, lets try to at least find the library
		find_library(LIB_SSH ssh2)
		target_link_libraries(isisImageFormat_sftp isis_core ${LIB_SSH} )
	endif(TARGET Libssh2::libssh2)

	set(TARGETS ${TARGETS} isisImageFormat_sftp)
endif(ISIS_IOPLUGIN_SFTP)

###########################################################################
# prepare all plugins for installation
###########################################################################
if(WIN32)
message(STATUS "Installing following plugins to ${CMAKE_INSTALL_PREFIX}/bin")
else()
message(STATUS "Installing following plugins to ${CMAKE_INSTALL_PREFIX}/${ISIS_PLUGIN_INFIX}")
endif()

foreach(PLUGIN ${TARGETS})
  message(STATUS " == ${PLUGIN}")
  set_target_properties(${PLUGIN} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
endforeach()

###########################################################
# actual install
###########################################################
if(WIN32)
install(TARGETS ${TARGETS} RUNTIME DESTINATION bin COMPONENT "IO_plugins")
else()
install(TARGETS ${TARGETS} DESTINATION ${ISIS_PLUGIN_INFIX} COMPONENT "IO_plugins" )
endif()

# # uninstall target
# configure_file(
# 	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
# 	"${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
# 	IMMEDIATE @ONLY
# )
