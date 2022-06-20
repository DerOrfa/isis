option(USE_SYSTEM_PYBIND "Use the system's pybind11 (instead of downloading it)" OFF)
set(PYBIND11_FINDPYTHON)

if(USE_SYSTEM_PYBIND)
    find_package(pybind11 CONFIG REQUIRED)
    include(pybind11Common)#conan doesn't load this automatically so we do it
    message(STATUS "Found pybind11 v${pybind11_VERSION}: ${pybind11_INCLUDE_DIRS}")
else(USE_SYSTEM_PYBIND)
    include(FetchContent)
    FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11
            GIT_TAG        v2.9.2
    )

    FetchContent_GetProperties(pybind11)
    if(NOT pybind11_POPULATED)
        FetchContent_Populate(pybind11)
        add_subdirectory(${pybind11_SOURCE_DIR} ${pybind11_BINARY_DIR})
    endif()
endif(USE_SYSTEM_PYBIND)

#watch out, this will fail if pybind::* came from conan and the conan build type doesnt fit that of cmake (Debug/Release)
pybind11_add_module(pyisis isis_python.cpp logging.cpp utils.cpp)
target_link_libraries(pyisis PRIVATE isis_core)

set(PYTHON_MODULE_PATH ${Python_SITELIB} CACHE PATH "Installation path for platform independent modules")
set(PYTHON_MODULE_PATH_ARCH ${Python_SITEARCH} CACHE PATH "Installation path for platform dependent modules")

message(STATUS "platform dependent python modules will be installed to ${PYTHON_MODULE_PATH_ARCH}")
install (TARGETS pyisis
        RUNTIME DESTINATION ${PYTHON_MODULE_PATH_ARCH} COMPONENT RuntimeLibraries
        LIBRARY DESTINATION ${PYTHON_MODULE_PATH_ARCH} COMPONENT RuntimeLibraries
)

#set_target_properties(pyisis PROPERTIES CXX_VISIBILITY_PRESET "hidden"	CUDA_VISIBILITY_PRESET "hidden")
