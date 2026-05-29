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

project(dal LANGUAGES CXX
    DESCRIPTION "DAL Multi-Project Workspace"
    VERSION 1.0.0)

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
