# dal-excel: Excel Add-in Interface
#
# This project provides the Excel COM add-in interface for DAL.
# It depends ONLY on dal-public (DAL::public), never directly on dal-cpp.
# Build is Windows-only.
#
# Target: dal_excel (alias DAL::excel)

cmake_minimum_required(VERSION 3.21.0)

project(dal-excel
    DESCRIPTION "DAL Excel Add-in"
    VERSION 1.0.0)

message(STATUS "-- Building dal-excel (refactored workspace)")

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_EXCEL_BUILD_TESTS "Build dal-excel tests" ON)

# ---------------------------------------------------------------------------
# Guard: dal-excel is Windows-only
# ---------------------------------------------------------------------------
if(NOT WIN32)
    message(STATUS "  dal-excel: skipping (requires Windows)")
    return()
endif()

# ---------------------------------------------------------------------------
# Dependency: dal-public (DAL::public)
# During Phase 1 scaffolding, may not be available -- be non-fatal.
# ---------------------------------------------------------------------------
if(TARGET DAL::public)
    message(STATUS "  dal-excel: using sibling DAL::public target")
else()
    find_package(dal-public QUIET)
    if(dal-public_FOUND)
        message(STATUS "  dal-excel: using installed dal-public package")
    else()
        message(STATUS "  dal-excel: DAL::public not available yet (placeholder scaffold)")
    endif()
endif()

# ---------------------------------------------------------------------------
# TODO(Phase 5): Move public/excel/ sources here.
# Target only links DAL::public.
# Tests use gtest + CTest.
# Currently this is a placeholder scaffold.
# ---------------------------------------------------------------------------

message(STATUS "  dal-excel placeholder scaffold -- Phase 5 will add Excel add-in sources")
