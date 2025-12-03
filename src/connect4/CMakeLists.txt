# Copyright 2025, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  connect4
  LANGUAGES CXX
  VERSION 0.1
)

#
# connect4 executable
#
add_executable(
  connect4
  main.cc
)

target_compile_features(connect4 PRIVATE cxx_std_17)
target_compile_options(
  connect4
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  connect4
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  connect4
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  connect4
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    connect4-library
)

install(
  TARGETS connect4
  RUNTIME DESTINATION ${CIFRE_INSTALL_PREFIX}/bin
)

#
# connect4 library
#
add_library(
  connect4-library STATIC
  connect4_board.cc
  connect4_player.cc
)

target_compile_features(connect4-library PRIVATE cxx_std_17)
target_compile_options(
  connect4-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  connect4-library
  PUBLIC
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  connect4-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME connect4
)

target_link_libraries(
  connect4-library
  PUBLIC
    cifre-library
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
)
