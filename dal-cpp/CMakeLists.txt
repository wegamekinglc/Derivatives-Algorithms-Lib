# dal-cpp: Core C++ Quant Library
#
# This project contains the full internal C++ implementation of the DAL library.
# It is the foundation for all other DAL sub-projects.
#
# Target: dal_cpp (alias DAL::cpp)

cmake_minimum_required(VERSION 3.21.0)

project(dal-cpp LANGUAGES CXX
    DESCRIPTION "DAL Core C++ Quant Library"
    VERSION 1.0.0)

message(STATUS "-- Building dal-cpp (refactored workspace)")

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_CPP_BUILD_TESTS "Build dal-cpp tests" ON)
option(DAL_CPP_BUILD_EXAMPLES "Build dal-cpp examples" ON)

# AAD backend selection (exactly one should be ON)
option(DAL_CPP_USE_XAD_AAD "Use XAD AAD backend" OFF)
option(DAL_CPP_USE_CODIPACK_AAD "Use CoDiPack AAD backend" OFF)
option(DAL_CPP_USE_ADEPT_AAD "Use Adept AAD backend" ON)

# ---------------------------------------------------------------------------
# TODO(Phase 2): Move dal/ sources, externals, config, tests, examples here.
# Currently this is a placeholder scaffold.
# ---------------------------------------------------------------------------

message(STATUS "  dal-cpp placeholder scaffold -- Phase 2 will add core sources")
