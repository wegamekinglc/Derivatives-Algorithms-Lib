cmake_minimum_required(VERSION 3.21.0)

# For MSVC RUNTIME LIBRARY, need CMP0091=NEW
cmake_policy(SET CMP0091 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

# Default build type
if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE
            STRING "Build type for single-config generators" FORCE)
endif()
message(STATUS "Build Mode: ${CMAKE_BUILD_TYPE}")

# C++17 must be set BEFORE project() so MSVC picks /std:c++17 for every
# sub-project and external. Without this, MSVC defaults to C++14 and
# rejects std::optional, nested namespaces, etc.
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS FALSE)

project(dal LANGUAGES CXX
    DESCRIPTION "DAL Multi-Project Workspace"
    VERSION 1.0.0)

# MSVC runtime library — must be set at workspace level so every
# sub-project's targets agree on /MT vs /MD. Without this, dal_cpp
# (built under dal-cpp/, which includes Platform.cmake) and
# dal_public_tests (built under dal-public/, which does not) end up
# with mismatched runtime flags and the linker rejects them.
if(MSVC)
    if(NOT DEFINED MSVC_RUNTIME)
        set(MSVC_RUNTIME "dynamic" CACHE STRING "MSVC runtime to link" FORCE)
    endif()
    if("${MSVC_RUNTIME}" STREQUAL "static")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    message(STATUS "MSVC_RUNTIME: ${MSVC_RUNTIME} (CMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})")

    add_compile_options(/utf-8)
endif()

# Coverage flags must be applied to ALL sub-projects so dal-public tests
# (which link against libdal_cpp.a) get the gcov runtime symbols. Setting
# them only inside dal-cpp leaves dal-public with undefined __gcov_init refs.
if(NOT DEFINED USE_COVERAGE)
    set(USE_COVERAGE FALSE)
endif()
if(USE_COVERAGE AND NOT MSVC)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
        add_link_options(-fprofile-instr-generate -fcoverage-mapping)
    else()
        add_compile_options(--coverage)
        add_link_options(--coverage)
    endif()
    message(STATUS "Coverage instrumentation enabled (USE_COVERAGE=${USE_COVERAGE})")
endif()

# Build all static libraries with -fPIC so they can be linked into the
# Python extension shared object (_dal.so) and any other shared library
# consumer. Without this, thread_local relocations (R_X86_64_TPOFF32)
# cause link failures when building shared objects.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Workspace-wide options
option(DAL_BUILD_PUBLIC "Build dal-public" ON)
option(DAL_BUILD_PYTHON "Build dal-python" OFF)
option(DAL_BUILD_EXCEL "Build dal-excel" OFF)

# Applied workspace-wide (like USE_COVERAGE): partially instrumented builds
# miss errors at library boundaries. -fno-sanitize-recover=all is deliberately
# left to the caller -- thread sanitizer has no recover mode.
set(DAL_ENABLE_SANITIZERS "" CACHE STRING
    "Semicolon-separated sanitizers for all targets (e.g. \"address;undefined\" or \"thread\"); empty disables instrumentation")
if(DAL_ENABLE_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR
            "DAL_ENABLE_SANITIZERS requires a GCC- or Clang-compatible compiler, "
            "got ${CMAKE_CXX_COMPILER_ID}")
    endif()
    string(REPLACE ";" "," DAL_SANITIZER_LIST "${DAL_ENABLE_SANITIZERS}")
    add_compile_options("-fsanitize=${DAL_SANITIZER_LIST}" -fno-omit-frame-pointer)
    add_link_options("-fsanitize=${DAL_SANITIZER_LIST}")
    message(STATUS "Sanitizer instrumentation enabled (DAL_ENABLE_SANITIZERS=${DAL_SANITIZER_LIST})")
endif()

enable_testing()

# Sub-projects
add_subdirectory(dal-cpp)

if(DAL_BUILD_PUBLIC)
    add_subdirectory(dal-public)
endif()

if(DAL_BUILD_PYTHON)
    add_subdirectory(dal-python)
endif()

if(DAL_BUILD_EXCEL)
    add_subdirectory(dal-excel)
endif()
