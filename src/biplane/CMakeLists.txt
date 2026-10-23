# Copyright 2026, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  biplane
  LANGUAGES CXX
  VERSION 0.1
)

#
# biplane executable
#
add_executable(
  biplane
  main.cc
)

target_compile_features(biplane PRIVATE cxx_std_17)
target_compile_options(
  biplane
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  biplane
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  biplane
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  biplane
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    biplane-library
)

install(
  TARGETS biplane
  RUNTIME DESTINATION ${CIFRE_INSTALL_PREFIX}/bin
)

#
# biplane library
#
add_library(
  biplane-library STATIC
  biplane_action.cc
  biplane_board.cc
  biplane_player.cc
)

target_compile_features(biplane-library PRIVATE cxx_std_17)
target_compile_options(
  biplane-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  biplane-library
  PUBLIC
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  biplane-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME biplane
)

target_link_libraries(
  biplane-library
  PUBLIC
    cifre-library
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
)
