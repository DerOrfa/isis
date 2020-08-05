############################################################
# The ISIS project
# 
# CMake configuration file for the lib subdir
#
# Author: Thomas Proeger <thomasproeger@googlemail.com>
# Date: Fri, 08 Apr 2011 13:11:34 +0200
# 
############################################################

############################################################
# optional components
############################################################
option(ISIS_QT5 "Enable adapter to the Qt5 library" OFF)

if(ISIS_QT5)
	set(ISIS_QT_VERSION_API "0.0.0")
	string( REGEX MATCH "^[0-9]+" ISIS_QT_VERSION_SO ${ISIS_QT_VERSION_API})

	find_package(Qt5Widgets)

	set(CMAKE_AUTOMOC ON)
	add_lib(isis_adapter_qt5
		"qt5/*.cpp" "qt5/*.hpp" "isis/adapter/qt5"
		"${isis_core_lib};Qt5::Widgets" "Threads::Threads"
		${ISIS_QT_VERSION_SO} ${ISIS_QT_VERSION_API}
	)
	#qt puts its moc-results into the build directory, we need to find them there
	target_include_directories(${isis_adapter_qt5_lib} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endif()

#option(ISIS_ITK "Enable adapter to the ITK library" OFF) #disabled for now

if(ISIS_ITK)
	find_package(ITK COMPONENTS ITKCommon ITKTransform CONFIG)

	#for some reason ITKConfig.cmake does not set these
	set_property(TARGET ITKCommon APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ITK_INCLUDE_DIRS})
	set_property(TARGET ITKTransform APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${ITK_INCLUDE_DIRS})

	add_lib(isis_adapter_itk4
		"itk4/*.cpp" "itk4/*.hpp" "isis/adapter/itk4"
		"${isis_core_lib};${isis_math_lib}" "ITKCommon;ITKTransform"
		"0.0.0" "0.0.0"
	)
endif()

# write configuration file
configure_file(isis_adaptercfg.cmake.in ${CMAKE_BINARY_DIR}/ISISAdapterConfig.cmake @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/ISISAdapterConfig.cmake DESTINATION share/isis/cmake COMPONENT Development)
