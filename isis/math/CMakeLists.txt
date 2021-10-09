############################################################
# export library dependencies of isis_core
############################################################

LIST(APPEND _DEPENDS ${Boost_LIBRARIES})

set(_CPP_GLOB "*.cpp" "details/details_fft.cpp")
set(_HPP_GLOB "*.hpp")

option(ISIS_USE_GSL "use gsl for math functions like fft" OFF)
option(ISIS_USE_FFTW "use FFTW for fft" OFF)
option(ISIS_USE_clFFT "use clFFT for fft" OFF)

if(ISIS_USE_GSL)
	if (PKG_CONFIG_FOUND)
		pkg_search_module(GSL REQUIRED gsl)
		include_directories(GSL_INCLUDE_DIRS)
		list(APPEND _DEPENDS ${GSL_LIBRARIES})
	else()
		find_package(GSL)
		include_directories(${GSL_INCLUDE_DIR})
		list(APPEND _DEPENDS ${GSL_LIBRARY})
	endif()

	list(APPEND _CPP_GLOB "gsl/*.cpp")
	list(APPEND _HPP_GLOB "gsl/*.hpp")

	add_definitions("-DHAVE_GSL")
endif()

if(ISIS_USE_FFTW)
	if(USE_CONAN) # conan creates a FindFFTW3.cmake for us
		find_package(FFTW3f REQUIRED)
		list(APPEND _DEPENDS FFTW3::fftw3f)
	elseif(PKG_CONFIG_FOUND)
		pkg_search_module(FFTW REQUIRED fftw3)
		pkg_search_module(FFTWf REQUIRED fftw3f)
		include_directories(${FFTW_INCLUDE_DIRS} ${FFTWf_INCLUDE_DIRS})
		list(APPEND _DEPENDS ${FFTW_LDFLAGS} ${FFTWf_LDFLAGS} ${FFTW_LIBRARIES} ${FFTWf_LIBRARIES})
		message(STATUS "fftw libraries: ${FFTW_LIBRARIES} ${FFTWf_LIBRARIES}")
		add_definitions("-DHAVE_FFTW")
	else()
		message(FATAL_ERROR "Implement fftw lookup")
	endif()

	list(APPEND _CPP_GLOB "details/fftw.cpp")

	add_definitions("-DHAVE_FFTWf")
endif()


if(ISIS_USE_clFFT)
	find_package(clFFT)
	list(APPEND _DEPENDS clFFT)
	list(APPEND _CPP_GLOB "details/opencl/*.cpp")
	list(APPEND _CPP_GLOB "details/clfft.cpp")
	add_definitions("-DHAVE_CLFFT")
endif()

############################################################
# build the math library
############################################################
add_lib(isis_math "${_CPP_GLOB}" "${_HPP_GLOB}" "isis/math" "isis_core" "${_DEPENDS}" ${ISIS_CORE_VERSION_SO} ${ISIS_CORE_VERSION_API})
