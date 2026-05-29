# dal-python: Python Bindings via SWIG
#
# This project generates Python bindings for the DAL public API using SWIG.
# It depends ONLY on dal-public (DAL::public), never directly on dal-cpp.
#
# Target: dal_python (alias DAL::python)

cmake_minimum_required(VERSION 3.21.0)

project(dal-python
    DESCRIPTION "DAL Python Bindings"
    VERSION 1.0.0)

message(STATUS "-- Building dal-python (refactored workspace)")

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_PYTHON_BUILD_TESTS "Build dal-python tests" ON)
option(DAL_PYTHON_BUILD_WHEEL "Build Python wheel" OFF)

# ---------------------------------------------------------------------------
# Dependency: dal-public (DAL::public)
# During Phase 1 scaffolding, may not be available -- be non-fatal.
# ---------------------------------------------------------------------------
if(TARGET DAL::public)
    message(STATUS "  dal-python: using sibling DAL::public target")
else()
    find_package(dal-public QUIET)
    if(dal-public_FOUND)
        message(STATUS "  dal-python: using installed dal-public package")
    else()
        message(STATUS "  dal-python: DAL::public not available yet (placeholder scaffold)")
    endif()
endif()

# ---------------------------------------------------------------------------
# TODO(Phase 4): Move public/swig/ and public/python/ here.
# SWIG interface should only #include <dal_public/...> headers.
# Tests use pytest.
# Currently this is a placeholder scaffold.
# ---------------------------------------------------------------------------

message(STATUS "  dal-python placeholder scaffold -- Phase 4 will add Python bindings")
