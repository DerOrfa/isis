include(FetchContent)
FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11
        GIT_TAG        v2.6.2
)

FetchContent_GetProperties(pybind11)
if(NOT pybind11_POPULATED)
    FetchContent_Populate(pybind11)
    add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
endif()

pybind11_add_module(pyisis isis_python.cpp logging.cpp utils.cpp)

#add_library(pyisis MODULE isis_python.cpp utils.cpp logging.cpp)
target_link_libraries(pyisis PRIVATE pybind11::module isis_core)
set_property(TARGET pyisis PROPERTY POSITION_INDEPENDENT_CODE ON)

find_package(Python)
set(PYTHON_MODULE_PATH ${Python_SITELIB} CACHE PATH "Installation path for platform independent modules")
set(PYTHON_MODULE_PATH_ARCH ${Python_SITEARCH} CACHE PATH "Installation path for platform dependent modules")

message(STATUS "platform dependent python modules will be installed to ${PYTHON_MODULE_PATH_ARCH}")
install (TARGETS pyisis
        RUNTIME DESTINATION ${PYTHON_MODULE_PATH_ARCH} COMPONENT RuntimeLibraries
        LIBRARY DESTINATION ${PYTHON_MODULE_PATH_ARCH} COMPONENT RuntimeLibraries
)

#pybind11_extension(pyisis)
#pybind11_strip(pyisis)
#set_target_properties(pyisis PROPERTIES CXX_VISIBILITY_PRESET "hidden"	CUDA_VISIBILITY_PRESET "hidden")
