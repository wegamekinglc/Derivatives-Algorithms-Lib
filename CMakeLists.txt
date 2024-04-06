cmake_minimum_required(VERSION 3.15.0)

# For MSVC RUNTIME LIBRARY, need CMP0091=NEW and cmake 3.15+
cmake_policy(SET CMP0091 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Version info
set(DAL_VERSION_MAJOR 1)
set(DAL_VERSION_MINOR 0)
set(DAL_VERSION_PATCH 0)
set(DAL_VERSION ${DAL_VERSION_MAJOR}.${DAL_VERSION_MINOR}.${DAL_VERSION_PATCH})

# Project Info
set(PACKAGE_NAME          "dal")
set(PACKAGE_VERSION       "${DAL_VERSION}-dev")
set(PACKAGE_VERSION_HEX   "0x01000000")
set(PACKAGE_STRING        "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME       "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT     "https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/issues/")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "") # works

# Default build type for single-config generators (set this before project() command)
# For multi-config generators, such as Visual Studio, use: cmake --build . --config=<CONFIG>
if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
    set(CMAKE_BUILD_TYPE "Release" CACHE
            STRING "Build type for single-config generators" FORCE)
endif()
message("-- Build Mode: ${CMAKE_BUILD_TYPE}")
message("-- MSVC_RUNTIME: ${MSVC_RUNTIME}")

project(${PACKAGE_NAME} LANGUAGES CXX DESCRIPTION "The DAL C++ Library")

# Path for package-local cmake modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Installation directories
set(DAL_INSTALL_BINDIR "bin" CACHE STRING "Installation directory for executables")
set(DAL_INSTALL_LIBDIR "lib" CACHE STRING "Installation directory for libraries")
set(DAL_INSTALL_INCLUDEDIR "include" CACHE STRING "Installation directory for headers")
set(DAL_INSTALL_EXAMPLESDIR "lib/dal/examples" CACHE STRING "Installation directory for examples")
set(DAL_INSTALL_CMAKEDIR "lib/cmake/${PACKAGE_NAME}" CACHE STRING "Installation directory for CMake scripts")

if (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_definitions(-DCODI_ForcedInlines)
    message("-- using forced inline for Codi")
endif()

# Project shared libs ON for UNIX
if (NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${UNIX})
endif()

# Require C++11 or higher
if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
elseif(CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "Please specify CMAKE_CXX_STANDARD of 17 or higher")
endif()
if (NOT DEFINED CMAKE_CXX_STANDARD_REQUIRED)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
# Avoid use of compiler language extensions, i.e. -std=c++11 not -std=gnu++11
if (NOT DEFINED CMAKE_CXX_EXTENSIONS)
    set(CMAKE_CXX_EXTENSIONS FALSE)
endif()

# If available, use PIC for shared libs and PIE for executables
if (NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if (CMAKE_POSITION_INDEPENDENT_CODE)
    # cmake policy CMP0083: add PIE support if possible (need cmake 3.14)
    include(CheckPIESupported)
    check_pie_supported()
endif()

set(DAL_VERSION ${PACKAGE_VERSION})
set(DAL_HEX_VERSION ${PACKAGE_VERSION_HEX})

if (NOT DEFINED SKIP_TESTS)
    set(SKIP_TESTS true)
endif()

if (NOT DEFINED USE_COVERAGE)
    set(USE_COVERAGE false)
endif()

include(Platform)

message("-- CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("-- CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("-- CMAKE_CXX_STANDARD: ${CMAKE_CXX_STANDARD}")
message("-- BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

find_package(RapidJSON CONFIG REQUIRED)
include_directories("${RapidJSON_INCLUDE_DIR}")
message("-- RapidJSON INCLUDE DIR: ${RapidJSON_INCLUDE_DIR}")
include_directories(${CMAKE_SOURCE_DIR})

if (DEFINED USE_CODI)
    include_directories(${CMAKE_SOURCE_DIR}/externals/CodiPack/include)
    add_compile_definitions(USE_CODI)
    message("-- USE AAD framework: CODI")
elseif(DEFINED USE_AADET)
    add_compile_definitions(USE_AADET)
    message("-- USE AAD framework: AADET")
ELSE (DEFINED USE_XAD)
    add_compile_definitions(USE_XAD)
    add_compile_definitions(XAD_USE_STRONG_INLINE)
    add_subdirectory(externals/xad)
    include_directories(${CMAKE_SOURCE_DIR}/externals/xad/include)
    message("-- USE AAD framework: XAD")
endif ()

add_subdirectory(dal)
add_subdirectory(public)
add_subdirectory(examples)

message("-- SKIP_TESTS: ${SKIP_TESTS}")
if ("${SKIP_TESTS}" STREQUAL "false")
    add_subdirectory(tests)
endif()

