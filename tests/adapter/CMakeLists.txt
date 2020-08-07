
if(ISIS_QT5)
	find_package(Qt5Widgets)
	add_executable( qt5test qt5test.cpp )
	target_link_libraries( qt5test isis_qt5 Boost::unit_test_framework)
	add_test(NAME qt5test COMMAND qt5test)
endif(ISIS_QT5)

if(ISIS_ITK)
	find_package(ITK REQUIRED ITKCommon ITKTransform ITKIONIFTI ITKIOImageBase)
	include(${ITK_USE_FILE})

	add_executable( itktest itktest.cpp )
	target_link_libraries( itktest isis_qt5 Boost::unit_test_framework ${isis_adapter_itk4_lib} ${ITK_LIBRARIES})
	add_test(NAME itktest COMMAND itktest)
endif(ISIS_ITK)

