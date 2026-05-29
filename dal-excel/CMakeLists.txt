# dal-excel: Excel Add-in Interface
#
# This project provides the Excel COM add-in interface for DAL.
# It depends ONLY on dal-public (DAL::public), never directly on dal-cpp.
#
# Build is Windows-only (requires Office COM DLLs). On non-Windows platforms
# this CMakeLists.txt returns early with a status message.
#
# Target: dal_excel (alias DAL::excel)
#
# Two build modes:
#   Standalone:  cmake -S dal-excel -B build -DCMAKE_PREFIX_PATH=<dal-public-install>
#   Monorepo:    add_subdirectory(dal-excel) from parent CMakeLists.txt

cmake_minimum_required(VERSION 3.21.0)

# ---------------------------------------------------------------------------
# Detect build mode
# ---------------------------------------------------------------------------
if(NOT DAL_EXCEL_AS_SUBDIRECTORY)
    set(DAL_EXCEL_IS_TOPLEVEL TRUE)
    message(STATUS "dal-excel: standalone top-level mode")
else()
    set(DAL_EXCEL_IS_TOPLEVEL FALSE)
    message(STATUS "dal-excel: subdirectory mode (added via add_subdirectory)")
endif()

# ---------------------------------------------------------------------------
# Project-level setup (standalone only)
# ---------------------------------------------------------------------------
if(DAL_EXCEL_IS_TOPLEVEL)
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    endif()

    project(dal-excel
        DESCRIPTION "DAL Excel Add-in"
        VERSION 1.0.0)

    message(STATUS "-- Build Mode: ${CMAKE_BUILD_TYPE}")

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS FALSE)
endif()

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_EXCEL_BUILD_TESTS "Build dal-excel tests" ON)

# ---------------------------------------------------------------------------
# Platform guard: dal-excel is Windows-only
# ---------------------------------------------------------------------------
if(NOT WIN32)
    message(STATUS "dal-excel: skipping build (requires Windows + Office COM DLLs)")
    return()
endif()

# ---------------------------------------------------------------------------
# Find the DAL public library
# ---------------------------------------------------------------------------
if(TARGET DAL::public)
    message(STATUS "dal-excel: linking against sibling DAL::public")
    set(DAL_PUBLIC_TARGET DAL::public)
elseif(TARGET dal_public)
    message(STATUS "dal-excel: linking against sibling dal_public")
    set(DAL_PUBLIC_TARGET dal_public)
elseif(TARGET dal_library)
    message(STATUS "dal-excel: linking against sibling dal_library (old build)")
    set(DAL_PUBLIC_TARGET dal_library)
else()
    if(DAL_EXCEL_IS_TOPLEVEL)
        find_package(dal-public REQUIRED)
        set(DAL_PUBLIC_TARGET DAL::public)
    else()
        message(FATAL_ERROR "dal-excel requires DAL::public or dal_public target")
    endif()
endif()

# ---------------------------------------------------------------------------
# Library target: dal_excel (alias DAL::excel)
#
# Mirrors the original public/excel/CMakeLists.txt exactly:
# - SHARED library built from all public/excel/*.hpp and *.cpp
# - Links against dal_public/DAL::public
# - .xll suffix (Excel add-in DLL)
# - Office COM compile definitions
# ---------------------------------------------------------------------------
file(GLOB_RECURSE EXCEL_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(dal_excel SHARED ${EXCEL_FILES})
add_library(DAL::excel ALIAS dal_excel)

if(MSVC)
    target_compile_definitions(dal_excel PRIVATE IS_BASE)
endif()

# Excel COM compile definitions (mirrors original)
target_compile_definitions(dal_excel PRIVATE
    USE_EXCEL_REPORT
    OFFICE_MSO_PATH="${OFFICE_MSO_DLL}"
    OFFICE_VBE_PATH="${OFFICE_VBE_OLB}"
    OFFICE_EXCEL_PATH="${OFFICE_EXCEL_EXE}"
)

# Link against the public library
target_link_libraries(dal_excel PUBLIC ${DAL_PUBLIC_TARGET})

# Excel add-in uses .xll extension
set_target_properties(dal_excel PROPERTIES
    SUFFIX .xll
    EXPORT_NAME excel
    OUTPUT_NAME dal_excel)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS dal_excel
        RUNTIME DESTINATION lib
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

# ---------------------------------------------------------------------------
# Test target: dal_excel_tests (Windows only)
# ---------------------------------------------------------------------------
if(DAL_EXCEL_BUILD_TESTS)
    enable_testing()

    if(NOT TARGET GTest::gtest_main)
        find_package(GTest REQUIRED)
    endif()

    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

    if(TEST_FILES)
        add_executable(dal_excel_tests ${TEST_FILES})
        target_link_libraries(dal_excel_tests PRIVATE dal_excel ${DAL_PUBLIC_TARGET})
        target_link_libraries(dal_excel_tests PRIVATE GTest::gtest_main)
        gtest_discover_tests(dal_excel_tests)
    else()
        message(STATUS "dal-excel: no tests found (tests/ directory is empty)")
    endif()
endif()
