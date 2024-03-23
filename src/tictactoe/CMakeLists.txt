# Copyright 2024, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  tictactoe
  LANGUAGES CXX
  VERSION 0.1
)

#
# tictactoe executable
#
add_executable(
  tictactoe
  main.cc
)

target_compile_features(tictactoe PRIVATE cxx_std_17)
target_compile_options(
  tictactoe
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  tictactoe
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  tictactoe
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  tictactoe
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    tictactoe-library
)

install(
  TARGETS tictactoe
  RUNTIME DESTINATION ${CIFRE_INSTALL_PREFIX}/bin
)

#
# tictactoe version
#
configure_file(tictactoe_version.h.in tictactoe_version.h)

add_library(
  tictactoe-version INTERFACE
)

target_include_directories(
  tictactoe-version
  INTERFACE
    ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  tictactoe-version
  INTERFACE
    cifre-version
    common-version
)

#
# tictactoe library
#
add_library(
  tictactoe-library STATIC
  tictactoe_board.cc
  tictactoe_player.cc
)

target_compile_features(tictactoe-library PRIVATE cxx_std_17)
target_compile_options(
  tictactoe-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  tictactoe-library
  PUBLIC
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  tictactoe-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME tictactoe
)

target_link_libraries(
  tictactoe-library
  PUBLIC
    tictactoe-version
    cifre-library
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
)
