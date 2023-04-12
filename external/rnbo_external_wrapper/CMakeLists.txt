# Copyright 2018 The Min-DevKit Authors. All rights reserved.
# Use of this source code is governed by the MIT License found in the License.md file.

cmake_minimum_required(VERSION 3.0)

set(AUTHOR_DOMAIN "com.cycling74")
set(COPYRIGHT_STRING "Copyright (c) 2020 Cycling '74")

include(${CONAN_MIN-API_ROOT}/script/min-pretarget.cmake)

set(RNBO_CPP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../)
set(RNBO_CLASS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../codegen/xam/test/test.cpp" CACHE FILEPATH "The file that holds the generated RNBO class code")
set(RNBO_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../codegen/xam/test/description.json" CACHE FILEPATH "path to description.json")

set(RNBO_WRAPPER_HAS_AUDIO ON CACHE BOOL "An indication of this external should be a audio~ external or not")
set(RNBO_WRAPPER_HAS_MSG_OUT ON CACHE BOOL "An indication of this external should have a message outlet")
set(RNBO_WRAPPER_INC_TRANSPORT_ATTR ON CACHE BOOL "Should this external have a transport attribute")

include(${RNBO_CPP_DIR}/cmake/RNBODescriptionHeader.cmake)
set(DESCRIPTION_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
rnbo_write_description_header_if_exists(${RNBO_DESCRIPTION_FILE} ${DESCRIPTION_INCLUDE_DIR})

include_directories(
	${RNBO_CPP_DIR}/
	${RNBO_CPP_DIR}/common/
	${RNBO_CPP_DIR}/src/
	${RNBO_CPP_DIR}/adapters/max/
	${RNBO_CPP_DIR}/src/3rdparty/
	${C74_INCLUDES}
	${DESCRIPTION_INCLUDE_DIR}
)
set(SOURCE_FILES
	rnbo_external_wrapper.cpp
	${RNBO_CLASS_FILE}
	${RNBO_CPP_DIR}/RNBO.cpp
	${RNBO_CPP_DIR}/adapters/max/RNBO_MaxPresetAdapter.cpp
	${RNBO_CPP_DIR}/adapters/max/rnbo_bufferref.cpp
	${RNBO_CPP_DIR}/adapters/max/rnbo_data_loader.cpp
)
add_library(${PROJECT_NAME} MODULE ${SOURCE_FILES})
include(${CONAN_MIN-API_ROOT}/script/min-posttarget.cmake)

add_definitions(-DRNBO_WRAPPER_MAX_NAME=${rnbo_external_wrapper_EXTERN_OUTPUT_NAME})
add_definitions(-DC74_MIN_NO_ENUM_CHECKS)

if (${RNBO_WRAPPER_HAS_AUDIO})
	add_definitions(-DRNBO_WRAPPER_HAS_AUDIO)
endif()
if (${RNBO_WRAPPER_HAS_MSG_OUT})
	add_definitions(-DRNBO_WRAPPER_HAS_MSG_OUT)
endif()
if (${RNBO_WRAPPER_INC_TRANSPORT_ATTR})
	add_definitions(-DRNBO_WRAPPER_INC_TRANSPORT_ATTR)
endif()

if (BUILD_SYSTEM_IS_MINGW)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE
		mingw_stdthreads
		-static-libgcc
		-static-libstdc++
	)
endif()
