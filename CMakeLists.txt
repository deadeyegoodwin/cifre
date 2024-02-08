# Copyright 2024, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  cifre
  LANGUAGES CXX
  VERSION 0.1
)

include(CMakeDependentOption)
include(FetchContent)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(ASAN "Enable ASAN" OFF)
set(COMMON_REPO_TAG "main" CACHE STRING "Tag for common repo")
set(TRITON_VERSION "24.01" CACHE STRING "Triton Inference Server version")

if(ASAN)
  set(CIFRE_ASAN_FLAGS "-fsanitize=address")
endif(ASAN)

#
# common
#
FetchContent_Declare(
  repo-common
  GIT_REPOSITORY git@github.com:deadeyegoodwin/common.git
  GIT_TAG ${COMMON_REPO_TAG}
  GIT_SHALLOW ON
)
FetchContent_MakeAvailable(repo-common)

#
# cifre executable
#
add_executable(
  cifre
  src/main.cc
)

target_compile_features(cifre PRIVATE cxx_std_17)
target_compile_options(
  cifre
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  cifre
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(
  cifre
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  cifre
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    cifre-library
)

#
# cifre version
#
configure_file(src/cifre_version.h.in cifre_version.h)

add_library(
  cifre-version INTERFACE
)

target_include_directories(
  cifre-version
  INTERFACE
    ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  cifre-version
  INTERFACE
    common-version
)

#
# cifre library
#
add_library(
  cifre-library STATIC
  src/cifre_nnet.cc
)

target_compile_features(cifre-library PRIVATE cxx_std_17)
target_compile_options(
  cifre-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  cifre-library
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(
  cifre-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME cifre
)

target_link_libraries(
  cifre-library
  PUBLIC
    cifre-version
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    cifre-triton
)

#
# Triton - extract libraries from nvcr image
#
set(TRITON_DOCKER_IMAGE "nvcr.io/nvidia/tritonserver:${TRITON_VERSION}-py3")
set(TRITON_CORE_LIBNAME "libtritonserver.so")
set(TRITON_ONNXRUNTIME_BACKEND_DIRNAME "onnxruntime")

add_custom_command(
  OUTPUT
    ${TRITON_CORE_LIBNAME}
    ${TRITON_ONNXRUNTIME_BACKEND_DIRNAME}
  COMMAND rm -fr backends && mkdir backends
  COMMAND docker pull ${TRITON_DOCKER_IMAGE}
  COMMAND docker rm cifre_triton || echo "error ignored..." || true
  COMMAND docker create --name cifre_triton ${TRITON_DOCKER_IMAGE}
  COMMAND docker cp cifre_triton:/opt/tritonserver/lib/${TRITON_CORE_LIBNAME} ${TRITON_CORE_LIBNAME}
  COMMAND docker cp cifre_triton:/opt/tritonserver/backends/${TRITON_ONNXRUNTIME_BACKEND_DIRNAME}/ backends
  COMMAND docker rm cifre_triton
  COMMENT "Extracting ${TRITON_CORE_LIBNAME} and ${TRITON_ONNXRUNTIME_BACKEND_DIRNAME} from ${TRITON_DOCKER_IMAGE}"
)

add_custom_target(
  cifre-triton-target
  DEPENDS
    ${TRITON_CORE_LIBNAME}
    ${TRITON_ONNXRUNTIME_BACKEND_DIRNAME}
)

add_library(cifre-triton SHARED IMPORTED GLOBAL)
add_dependencies(cifre-triton cifre-triton-target)
set_target_properties(
  cifre-triton
  PROPERTIES
    IMPORTED_LOCATION ${TRITON_CORE_LIBNAME}
)

#
# Test target
#
#add_custom_target(
#  cifre-test
#  COMMAND ${CMAKE_CTEST_COMMAND} -j4 --timeout 5 --output-on-failure
#  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/test
#  COMMENT "Running cifre tests"
#  DEPENDS cifre
#)
#add_subdirectory(test)
