# Copyright 2025, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  gettysburg
  LANGUAGES CXX
  VERSION 0.1
)

#
# gettysburg executable
#
add_executable(
  gettysburg
  main.cc
)

target_compile_features(gettysburg PRIVATE cxx_std_17)
target_compile_options(
  gettysburg
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  gettysburg
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  gettysburg
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  gettysburg
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    gettysburg-library
)

install(
  TARGETS gettysburg
  RUNTIME DESTINATION ${CIFRE_INSTALL_PREFIX}/bin
)

#
# gettysburg library
#
add_library(
  gettysburg-library STATIC
  gettysburg_board.cc
  gettysburg_board_cell_factory.cc
  gettysburg_oob.cc
  gettysburg_player.cc
  gettysburg_unit.cc
)

target_compile_features(gettysburg-library PRIVATE cxx_std_17)
target_compile_options(
  gettysburg-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  gettysburg-library
  PUBLIC
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  gettysburg-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME gettysburg
)

target_link_libraries(
  gettysburg-library
  PUBLIC
    cifre-library
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
)
