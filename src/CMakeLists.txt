cmake_minimum_required(VERSION 2.8)
project(EFFICIENT)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)

IF(NOT CMAKE_BUILD_TYPE)
    message(STATUS "CMAKE_BUILD_TYPE is not set, set to default = RELWITHDEBINFO")
    set(CMAKE_BUILD_TYPE            "RELWITHDEBINFO")
ENDIF()
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo;Debug;Release;MinSizeRel" CACHE STRING "" FORCE)

IF (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64.*|AARCH64.*)")
    set(AARCH64 1)
ENDIF()
IF (NOT AARCH64)
    set(MACHINE_FLAGS "-msse4 -mpopcnt -fopenmp")
ENDIF()

set(COMMON_WARNING_FLAGS "-Wall -Werror -Wnon-virtual-dtor -Wno-sign-compare -Wno-unused-variable -Wno-unused-function -Wno-unused-result")

set(CMAKE_BUILD_COLOR_MAKEFILE          ON)
set(CMAKE_CXX_FLAGS "-std=gnu++1y ${COMMON_WARNING_FLAGS} ${MACHINE_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE             "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO      "-O3 -g")
set(CMAKE_EXE_LINKER_FLAGS              "-static-libgcc -static-libstdc++")

# cmake -DCMAKE_BUILD_TYPE=Debug ..
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

message(STATUS "CXX_FLAGS:=${CMAKE_CXX_FLAGS}")

message(STATUS "CMAKE_MODULE_PATH:=${CMAKE_MODULE_PATH}")

option(BUILD_TESTS "If enabled, compile the tests." OFF)

if (BUILD_TESTS)
  find_package(GMock MODULE REQUIRED)
endif(BUILD_TESTS)

find_package(Boost 1.53.0 REQUIRED COMPONENTS thread system)
find_package(Protobuf REQUIRED)
find_package(Grpc REQUIRED)
find_package(Threads REQUIRED)
find_package(Unwind)
find_package(BFD)

if (Unwind_FOUND)
  add_definitions(-DUSE_LIBUNWIND)
else()
  set(Unwind_INCLUDE_DIR "")
  set(Unwind_STATIC_LIBS "")
endif()

if (BFD_FOUND)
  #add_definitions(-DUSE_LIBBFD)
else()
  set(BFD_INCLUDE_DIR "")
  set(BFD_STATIC_LIBS "")
endif()

file(GLOB_RECURSE EFFICIENT_SOURCE_FILES "*.hpp" "*.cpp" "*.cc")

add_library(efficient ${EFFICIENT_SOURCE_FILES})

target_include_directories(efficient PUBLIC
        ${MODEL_DIR}
        ${BFD_INCLUDE_DIR}
        ${Unwind_INCLUDE_DIR})
target_link_libraries(efficient PUBLIC
        z rt nsl
        ${CMAKE_THREAD_LIBS_INIT}
        ${BFD_STATIC_LIBS}
        ${Unwind_STATIC_LIBS}
        ${Boost_LIBRARIES})
