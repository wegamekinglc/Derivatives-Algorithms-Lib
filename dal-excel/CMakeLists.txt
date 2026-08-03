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
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
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
set(DAL_EXCEL_USES_INSTALLED_PUBLIC FALSE)
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
        find_package(dal-public CONFIG REQUIRED)
        set(DAL_PUBLIC_TARGET DAL::public)
        set(DAL_EXCEL_USES_INSTALLED_PUBLIC TRUE)
    else()
        message(FATAL_ERROR "dal-excel requires DAL::public or dal_public target")
    endif()
endif()

# ---------------------------------------------------------------------------
# Library target: dal_excel (alias DAL::excel)
#
# Mirrors the original public/excel/CMakeLists.txt:
# - SHARED library built from all dal-excel/src/*.hpp and *.cpp
# - Auto-generated *.inc files under dal-excel/auto/ (from Machinist)
# - Links against dal_public/DAL::public
# - .xll suffix (Excel add-in DLL)
# - Office COM compile definitions
# ---------------------------------------------------------------------------
file(GLOB_RECURSE EXCEL_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/auto/*.inc")

add_library(dal_excel SHARED ${EXCEL_FILES})
add_library(DAL::excel ALIAS dal_excel)
target_include_directories(dal_excel PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")

if(DAL_EXCEL_USES_INSTALLED_PUBLIC)
    if(NOT COMMAND dal_cpp_apply_msvc_runtime)
        message(FATAL_ERROR "installed dal-cpp package did not publish dal_cpp_apply_msvc_runtime")
    endif()
    dal_cpp_apply_msvc_runtime(dal_excel)
    if(MSVC)
        get_target_property(DAL_EXCEL_MSVC_RUNTIME dal_excel MSVC_RUNTIME_LIBRARY)
        if(NOT "${DAL_EXCEL_MSVC_RUNTIME}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
            message(FATAL_ERROR
                "dal_excel runtime '${DAL_EXCEL_MSVC_RUNTIME}' does not match "
                "installed DAL runtime '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
        endif()
    endif()
endif()

if(MSVC)
    target_compile_options(dal_excel PRIVATE /utf-8)
    target_compile_definitions(dal_excel PRIVATE
        IS_BASE
        # Windows.h defines min/max as macros, which breaks AAD headers
        # (XAD, Adept) that use std::numeric_limits<...>::max() and
        # std::min/max. NOMINMAX suppresses those macros.
        NOMINMAX
    )
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
    target_compile_definitions(dal_excel PRIVATE DAL_EXCEL_TEST_API_EXPORTS)
    enable_testing()

    if(NOT TARGET GTest::gtest_main)
        find_package(GTest REQUIRED)
    endif()

    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

    if(TEST_FILES)
        add_executable(dal_excel_tests ${TEST_FILES})
        target_compile_definitions(dal_excel_tests PRIVATE DAL_EXCEL_TEST_API_IMPORTS)
        if(DAL_EXCEL_USES_INSTALLED_PUBLIC)
            dal_cpp_apply_msvc_runtime(dal_excel_tests)
            if(MSVC)
                get_target_property(DAL_EXCEL_TESTS_MSVC_RUNTIME dal_excel_tests MSVC_RUNTIME_LIBRARY)
                if(NOT "${DAL_EXCEL_TESTS_MSVC_RUNTIME}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
                    message(FATAL_ERROR
                        "dal_excel_tests runtime '${DAL_EXCEL_TESTS_MSVC_RUNTIME}' does not match "
                        "installed DAL runtime '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
                endif()
            endif()
        endif()
        target_link_libraries(dal_excel_tests PRIVATE dal_excel ${DAL_PUBLIC_TARGET})
        target_link_libraries(dal_excel_tests PRIVATE GTest::gtest_main)
        gtest_discover_tests(dal_excel_tests)
    else()
        message(STATUS "dal-excel: no tests found (tests/ directory is empty)")
    endif()
endif()
