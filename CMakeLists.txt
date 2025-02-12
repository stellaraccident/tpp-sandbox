message(STATUS "TPP Project CMakeLists.txt BEGIN")

cmake_minimum_required(VERSION 3.13.4)

# CMP0116: Ninja generators transform `DEPFILE`s from `add_custom_command()`
# New in CMake 3.20. https://cmake.org/cmake/help/latest/policy/CMP0116.html
if(POLICY CMP0116)
  cmake_policy(SET CMP0116 OLD)
endif()

project(standalone-dialect LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")

find_package(MLIR REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)
set(MLIR_BINARY_DIR ${CMAKE_BINARY_DIR})

option(TPP_INSIDE_IREE "Enables being built inside IREE" OFF)

if (NOT TPP_INSIDE_IREE)
  message(STATUS "Enabling standalone tooling")
  message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
else()
  message(STATUS "Enabling IREE tooling")
  message(STATUS "Using MLIRConfig.cmake in: ${MLIR_MAIN_SRC_DIR}/cmake/modules")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_MAIN_SRC_DIR}/cmake")
  list(APPEND CMAKE_MODULE_PATH ${MLIR_MAIN_SRC_DIR}/cmake/modules)
  list(APPEND CMAKE_MODULE_PATH ${LLVM_MAIN_SRC_DIR}/cmake)
endif()
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/modules")

include(TableGen)
include(AddLLVM)
include(AddMLIR)
include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/include)
link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

set(LLVM_LIT_ARGS "-sv" CACHE STRING "lit default options")
include(sanitizers)

# libxsmm
include(xsmm)
message (STATUS ${XSMM_INCLUDE_DIRS})

# Unit test
set(LLVM_MAIN_SRC_DIR ${LLVM_MAIN_INCLUDE_DIR}/../)
message(STATUS "Using LLVM: ${LLVM_MAIN_SRC_DIR}")
add_custom_target(StandaloneUnitTests)
if (EXISTS ${LLVM_MAIN_SRC_DIR}/utils/unittest/googletest/include/gtest/gtest.h)
  add_subdirectory(unittests)
else()
  message(WARNING "gtest not found, unittests will not be available")
endif()

add_subdirectory(include)
add_subdirectory(lib)

# IREE doesn't need any of these
if (NOT TPP_INSIDE_IREE)
  add_subdirectory(test)
  add_subdirectory(standalone-opt)
  add_subdirectory(standalone-rt)
endif()

message(STATUS "TPP Project CMakeLists.txt END")
