# Copyright 2024, David Goodwin. All rights reserved.

cmake_minimum_required(VERSION 3.18)

project(
  celcath
  LANGUAGES CXX
  VERSION 0.1
)

#
# celcath executable
#
add_executable(
  celcath
  main.cc
)

target_compile_features(celcath PRIVATE cxx_std_17)
target_compile_options(
  celcath
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  celcath
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  celcath
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    SKIP_BUILD_RPATH TRUE
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH FALSE
    INSTALL_RPATH "$\{ORIGIN\}"
)

target_link_libraries(
  celcath
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
    celcath-library
)

install(
  TARGETS celcath
  RUNTIME DESTINATION ${CIFRE_INSTALL_PREFIX}/bin
)

#
# celcath version
#
configure_file(celcath_version.h.in celcath_version.h)

add_library(
  celcath-version INTERFACE
)

target_include_directories(
  celcath-version
  INTERFACE
    ${PROJECT_BINARY_DIR}
)

target_link_libraries(
  celcath-version
  INTERFACE
    cifre-version
    common-version
)

#
# celcath library
#
add_library(
  celcath-library STATIC
  celcath_action.cc
  celcath_board.cc
  celcath_player.cc
  celcath_settings.cc
  celcath_stepper.cc
)

target_compile_features(celcath-library PRIVATE cxx_std_17)
target_compile_options(
  celcath-library
  PRIVATE
    -Wall -Wextra -Werror -Wno-unused-parameter
    ${CIFRE_ASAN_FLAGS}
)

target_include_directories(
  celcath-library
  PUBLIC
    ${PROJECT_SOURCE_DIR}
)

set_target_properties(
  celcath-library
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME celcath
)

target_link_libraries(
  celcath-library
  PUBLIC
    celcath-version
    cifre-library
  PRIVATE
    ${CIFRE_ASAN_FLAGS}
    common-library
)
