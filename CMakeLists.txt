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
endif()

# Workspace-wide options
option(DAL_BUILD_PUBLIC "Build dal-public" ON)
option(DAL_BUILD_PYTHON "Build dal-python" OFF)
option(DAL_BUILD_EXCEL "Build dal-excel" OFF)

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
