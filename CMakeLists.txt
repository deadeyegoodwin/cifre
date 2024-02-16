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
option(BUILD_TRITON_CLIENT "Build Triton Client Library" ON)
set(COMMON_REPO_TAG "main" CACHE STRING "Tag for common repo")
set(TRITON_REPO_TAG "r24.01" CACHE STRING "Tag for Triton repos")

if(ASAN)
  set(CIFRE_ASAN_FLAGS "-fsanitize=address")
endif(ASAN)

# Can't use CMAKE_INSTALL_PREFIX directly because that causes
# interactions with FetchContent projects and their dependencies...
set(CIFRE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install)
file(MAKE_DIRECTORY ${CIFRE_INSTALL_PREFIX}/include)
file(MAKE_DIRECTORY ${CIFRE_INSTALL_PREFIX}/lib)
file(MAKE_DIRECTORY ${CIFRE_INSTALL_PREFIX}/bin)

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
# Triton client
#
if(BUILD_TRITON_CLIENT)
  set(TRITON_ENABLE_CC_HTTP ON)
  set(TRITON_COMMON_REPO_TAG ${TRITON_REPO_TAG})
  set(TRITON_THIRD_PARTY_REPO_TAG ${TRITON_REPO_TAG})
  set(TRITON_CORE_REPO_TAG ${TRITON_REPO_TAG})

  # Triton client build has unnecessary dependency for http client that
  # causes large grpc repo to be fetched. Patch to avoid that...
  set(TRITON_CLIENT_PATCH git apply ${CMAKE_CURRENT_SOURCE_DIR}/patch/triton_client.patch)

  FetchContent_Declare(
    repo-triton-client
    GIT_REPOSITORY https://github.com/triton-inference-server/client.git
    GIT_TAG ${TRITON_REPO_TAG}
    GIT_SHALLOW ON
    PATCH_COMMAND ${TRITON_CLIENT_PATCH}
    UPDATE_DISCONNECTED 1
  )
  FetchContent_MakeAvailable(repo-triton-client)

  set(TRITON_CLIENT_INSTALL_DIR ${CMAKE_BINARY_DIR}/_deps/repo-triton-client-build/cc-clients/install)
  set(TRITON_CURL_LIBDIR ${CMAKE_BINARY_DIR}/_deps/repo-triton-client-build/third-party/curl/lib)

  add_custom_target(
    triton-client-target
    COMMAND cp -r ${TRITON_CLIENT_INSTALL_DIR}/lib/libhttpclient_static.a ${CIFRE_INSTALL_PREFIX}/lib/.
    COMMAND cp -r ${TRITON_CURL_LIBDIR}/libcurl.a ${CIFRE_INSTALL_PREFIX}/lib/.
    COMMAND cp -r ${TRITON_CLIENT_INSTALL_DIR}/include/common.h ${CIFRE_INSTALL_PREFIX}/include/.
    COMMAND cp -r ${TRITON_CLIENT_INSTALL_DIR}/include/ipc.h ${CIFRE_INSTALL_PREFIX}/include/.
    COMMAND cp -r ${TRITON_CLIENT_INSTALL_DIR}/include/http_client.h ${CIFRE_INSTALL_PREFIX}/include/.
    COMMENT "Installing Triton client artifacts into ${CIFRE_INSTALL_PREFIX}"
  )
  add_dependencies(triton-client-target cc-clients)
endif(BUILD_TRITON_CLIENT)

add_library(triton-client-curl-library STATIC IMPORTED GLOBAL)
set_target_properties(
  triton-client-curl-library
  PROPERTIES
    IMPORTED_LOCATION ${CIFRE_INSTALL_PREFIX}/lib/libcurl.a
)

add_library(triton-client-http-library STATIC IMPORTED GLOBAL)
set_target_properties(
  triton-client-http-library
  PROPERTIES
    IMPORTED_LOCATION ${CIFRE_INSTALL_PREFIX}/lib/libhttpclient_static.a
    INTERFACE_INCLUDE_DIRECTORIES ${CIFRE_INSTALL_PREFIX}/include
)

add_library(triton-client-library INTERFACE)
target_link_libraries(
  triton-client-library
  INTERFACE
    triton-client-http-library
    triton-client-curl-library
    ssl
    crypto
    z
)

if(BUILD_TRITON_CLIENT)
  add_dependencies(triton-client-library triton-client-target)
endif(BUILD_TRITON_CLIENT)

#
# Games
#
add_subdirectory(src/celcath)

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
  src/cifre_game.cc
  src/cifre_triton.cc
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
  PUBLIC
    ${CMAKE_SOURCE_DIR}/src
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
    triton-client-library
)
