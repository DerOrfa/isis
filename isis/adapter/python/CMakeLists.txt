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

add_library(isis MODULE chunk_python.cpp utils.cpp)

target_link_libraries(isis PRIVATE pybind11::module isis_core)

set_property(TARGET isis PROPERTY POSITION_INDEPENDENT_CODE ON)

pybind11_extension(isis)
pybind11_strip(isis)

set_target_properties(isis PROPERTIES CXX_VISIBILITY_PRESET "hidden"	CUDA_VISIBILITY_PRESET "hidden")
