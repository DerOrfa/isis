
# look for itk package
find_package(ITK)
if(ITK_FOUND)
  include(${ITK_USE_FILE})
else()
  message(FATAL_ERROR
    "ITK not found. Please set ITK_DIR.")
endif()


add_executable(extRegistrationTest registrationFactory3DTest.cpp)
target_link_libraries(extRegistrationTest ITKIO ITKSpatialObject ITKNumerics ITKStatistics)



