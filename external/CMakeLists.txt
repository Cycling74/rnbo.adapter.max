cmake_minimum_required(VERSION 3.19)
project(RNBO-MaxExternalWrapper)

if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release." FORCE)
endif()

include(../../../cmake/RNBOConan.cmake)
include(../../../cmake/MinGWSTDThreads.cmake)

set(CONAN_PROFILE "default" CACHE STRING "The profile to use for building conan deps, this is useful for cross compiling")

#windows needs a PACKAGE_VERSION, could we get this from some meta data?
set(PACKAGE_VERSION 1.0.0)

if (${CMAKE_GENERATOR} MATCHES "Xcode")
 		if (${XCODE_VERSION} VERSION_LESS 7)
  			message(STATUS "Xcode 7 or 8 is required. Please install from the Mac App Store.")
			return ()
		endif ()
endif ()

#override some settings
set(C74_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/externals/" CACHE FILEPATH "The destination path for the built external")

set(EXTERN_OUTPUT_NAME "rnbo_external_wrapper~" CACHE STRING "name of external")
set(rnbo_external_wrapper_EXTERN_OUTPUT_NAME "${EXTERN_OUTPUT_NAME}" CACHE STRING "name of external" FORCE)

conan_cmake_configure(
	REQUIRES min-api/xnor_build-updates_c1732da077fb2313a937ebf7d7975c6b@xnor/testing
	GENERATORS cmake_paths
)
conan_cmake_autodetect(settings)
conan_cmake_install(
	PATH_OR_REFERENCE .
	BUILD missing
	PROFILE_HOST ${CONAN_PROFILE}
	PROFILE_BUILD default
	SETTINGS_HOST build_type=${CMAKE_BUILD_TYPE}
	SETTINGS_BUILD build_type=${CMAKE_BUILD_TYPE}
	REMOTE cycling-public
)
include(${CMAKE_CURRENT_BINARY_DIR}/conan_paths.cmake)
include(${CONAN_MIN-API_ROOT}/script/min-package.cmake)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rnbo_external_wrapper)

#cloud compiler uses install to locate artifacts
install(TARGETS rnbo_external_wrapper
	LIBRARY
	DESTINATION .
)
