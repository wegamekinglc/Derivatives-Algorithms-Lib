# dal-public: Stable Public C++ API Layer
#
# This project wraps dal-cpp internals behind a stable, controlled public API.
# Downstream projects (dal-python, dal-excel) depend only on this layer.
#
# Target: dal_public (alias DAL::public)

cmake_minimum_required(VERSION 3.21.0)

project(dal-public LANGUAGES CXX
    DESCRIPTION "DAL Public C++ API"
    VERSION 1.0.0)

message(STATUS "-- Building dal-public (refactored workspace)")

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_PUBLIC_BUILD_TESTS "Build dal-public tests" ON)

# ---------------------------------------------------------------------------
# Dependency: dal-cpp
#
# In monorepo mode, DAL::cpp is available as a sibling target.
# In installed mode, use find_package.
# During Phase 1 scaffolding, neither may be available -- be non-fatal.
# ---------------------------------------------------------------------------
if(TARGET DAL::cpp)
    message(STATUS "  dal-public: using sibling DAL::cpp target")
else()
    find_package(dal-cpp QUIET)
    if(dal-cpp_FOUND)
        message(STATUS "  dal-public: using installed dal-cpp package")
    else()
        message(STATUS "  dal-public: DAL::cpp not available yet (placeholder scaffold)")
    endif()
endif()

# ---------------------------------------------------------------------------
# TODO(Phase 3): Move public/src/ sources, public/auto/, config here.
# Currently this is a placeholder scaffold.
# ---------------------------------------------------------------------------

message(STATUS "  dal-public placeholder scaffold -- Phase 3 will add public API sources")
