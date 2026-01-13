# Copyright 2026, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  othello
  LANGUAGES CXX
  VERSION 0.1
)

#
# othello executable
#
add_executable(
  othello
  main.cc
)

target_compile_features(othello PRIVATE cxx_std_17)
target_compile_options(
  othello
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  othello
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  othello
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  othello
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    othello-library
)

install(
  TARGETS othello
  RUNTIME DESTINATION ${CIFRE_INSTALL_PREFIX}/bin
)

#
# othello library
#
add_library(
  othello-library STATIC
  othello_board.cc
  othello_player.cc
)

target_compile_features(othello-library PRIVATE cxx_std_17)
target_compile_options(
  othello-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  othello-library
  PUBLIC
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  othello-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME othello
)

target_link_libraries(
  othello-library
  PUBLIC
    cifre-library
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
)
