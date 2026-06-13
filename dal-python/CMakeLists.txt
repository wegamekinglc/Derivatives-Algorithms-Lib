# dal-python: Python Bindings via pybind11
#
# This project generates Python bindings for the DAL public API using pybind11.
# It depends ONLY on dal-public (DAL::public), never directly on dal-cpp.
#
# Target: dal_python (alias DAL::python)
#
# Two build modes:
#   Standalone:  cmake -S dal-python -B build -DDAL_DIR=<dal-install-root>
#   Monorepo:    add_subdirectory(dal-python) from parent CMakeLists.txt

cmake_minimum_required(VERSION 3.21.0)

# ---------------------------------------------------------------------------
# Detect build mode
# ---------------------------------------------------------------------------
if(NOT DAL_PYTHON_AS_SUBDIRECTORY)
    set(DAL_PYTHON_IS_TOPLEVEL TRUE)
    message(STATUS "dal-python: standalone top-level mode")
else()
    set(DAL_PYTHON_IS_TOPLEVEL FALSE)
    message(STATUS "dal-python: subdirectory mode (added via add_subdirectory)")
endif()

# ---------------------------------------------------------------------------
# Project-level setup (standalone only)
# ---------------------------------------------------------------------------
if(DAL_PYTHON_IS_TOPLEVEL)
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    endif()

    project(dal-python
        DESCRIPTION "DAL Python Bindings"
        VERSION 1.0.0)

    message(STATUS "-- Build Mode: ${CMAKE_BUILD_TYPE}")

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS FALSE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()
endif()

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_PYTHON_BUILD_TESTS "Build dal-python tests" ON)
option(DAL_PYTHON_BUILD_WHEEL "Build Python wheel" OFF)

# ---------------------------------------------------------------------------
# Dependencies: pybind11, Python3, and dal-public
# ---------------------------------------------------------------------------
# Find Python3 FIRST so pybind11's internal FindPythonLibsNew reuses the
# same interpreter and include paths (avoids picking up a system Python
# without dev headers).
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "-- Python3: ${Python3_EXECUTABLE} (version ${Python3_VERSION})")

# Find pybind11: try system-installed first, then FetchContent fallback
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    message(STATUS "-- pybind11: not found via find_package, using FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)
    message(STATUS "-- pybind11: fetched v2.11.1")
else()
    message(STATUS "-- pybind11: found via find_package (${pybind11_VERSION})")
endif()

# Find the DAL public library
if(TARGET DAL::public)
    message(STATUS "-- core: linking against sibling DAL::public")
    set(DAL_PUBLIC_TARGET DAL::public)
elseif(TARGET dal_public)
    message(STATUS "-- core: linking against sibling dal_public")
    set(DAL_PUBLIC_TARGET dal_public)
else()
    if(DAL_PYTHON_IS_TOPLEVEL)
        # Try find_package first
        find_package(dal-public QUIET)
        if(TARGET DAL::public)
            set(DAL_PUBLIC_TARGET DAL::public)
        elseif(DAL_DIR)
            find_package(dal-public QUIET PATHS "${DAL_DIR}" NO_DEFAULT_PATH)
            if(TARGET DAL::public)
                message(STATUS "-- core: linking against DAL::public from ${DAL_DIR}")
                set(DAL_PUBLIC_TARGET DAL::public)
            else()
            # Determine library prefix/suffix for the platform
            if(WIN32)
                set(DAL_LIB_PREFIX "")
                set(DAL_LIB_SUFFIX ".lib")
            else()
                set(DAL_LIB_PREFIX "lib")
                set(DAL_LIB_SUFFIX ".a")
            endif()

            set(DAL_PUBLIC_LIB "${DAL_DIR}/lib/${DAL_LIB_PREFIX}dal_public${DAL_LIB_SUFFIX}")
            set(DAL_CPP_LIB    "${DAL_DIR}/lib/${DAL_LIB_PREFIX}dal_cpp${DAL_LIB_SUFFIX}")

            # Thread library for platform
            if(NOT WIN32)
                set(DAL_THREAD_LIB ";pthread")
            else()
                set(DAL_THREAD_LIB "")
            endif()

            if(EXISTS "${DAL_PUBLIC_LIB}" AND EXISTS "${DAL_CPP_LIB}")
                message(STATUS "-- core: using pre-built DAL libraries from ${DAL_DIR}")

                add_library(dal_cpp_static STATIC IMPORTED)
                set_target_properties(dal_cpp_static PROPERTIES
                    IMPORTED_LOCATION "${DAL_CPP_LIB}")

                add_library(dal_public_static STATIC IMPORTED)
                set_target_properties(dal_public_static PROPERTIES
                    IMPORTED_LOCATION "${DAL_PUBLIC_LIB}"
                    INTERFACE_LINK_LIBRARIES "dal_cpp_static${DAL_THREAD_LIB}")

                include_directories("${DAL_DIR}/include")

                set(DAL_PUBLIC_TARGET dal_public_static)
            else()
                message(FATAL_ERROR "dal-python: expected both DAL libraries in ${DAL_DIR}/lib/: ${DAL_PUBLIC_LIB} and ${DAL_CPP_LIB}")
            endif()
            endif()
        else()
            message(FATAL_ERROR "dal-python requires DAL::public or DAL_DIR pointing at an installed DAL tree")
        endif()
    else()
        message(FATAL_ERROR "dal-python requires dal-public (DAL::public or dal_public) target")
    endif()
endif()

# ---------------------------------------------------------------------------
# Include paths
# In monorepo mode, the parent already sets include_directories to the repo
# root, so <dal/...> and <dal-public/src/...> includes work.
# In standalone mode, we need to set them up.
# ---------------------------------------------------------------------------
if(DAL_PYTHON_IS_TOPLEVEL)
    if(DAL_DIR)
        # Repo root so <dal/...> and <dal-public/src/...> includes resolve.
        include_directories("${DAL_DIR}")
        include_directories("${DAL_DIR}/include")
    else()
        include_directories(${PROJECT_SOURCE_DIR}/..)
    endif()
endif()

# ---------------------------------------------------------------------------
# Python module: _dal (built via pybind11)
#
# The pybind11 bindings are split across domain-organized source files under
# src/bindings/.  module.cpp is the PYBIND11_MODULE entry point that calls
# init functions from the individual domain files.
# The compiled module is named _dal and output to <build>/dal/.
# A thin dal.py shim re-exports everything from _dal.
# ---------------------------------------------------------------------------

pybind11_add_module(_dal
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/module.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/global.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/models.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/random.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/script.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/value.cpp)

add_library(DAL::python ALIAS _dal)

# Link against the public library
if(DAL_PUBLIC_TARGET)
    target_link_libraries(_dal PRIVATE ${DAL_PUBLIC_TARGET})
endif()

# Set Python module properties
target_include_directories(_dal PRIVATE ${Python3_INCLUDE_DIRS})

# AAD backend include paths for the pybind11 wrapper compilation.
if(DAL_USE_ADEPT_AAD OR DAL_USE_XAD_AAD)
    if(DAL_DIR)
        target_include_directories(_dal PRIVATE "${DAL_DIR}/include")
    else()
        target_include_directories(_dal PRIVATE "${CMAKE_INSTALL_PREFIX}/include")
    endif()
endif()

if(WIN32)
    set(DAL_PYTHON_MODULE_SUFFIX ".${Python3_SOABI}.pyd")
else()
    set(DAL_PYTHON_MODULE_SUFFIX ".${Python3_SOABI}${CMAKE_SHARED_MODULE_SUFFIX}")
endif()

set_target_properties(_dal PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dal
    PREFIX ""
    OUTPUT_NAME _dal
    SUFFIX "${DAL_PYTHON_MODULE_SUFFIX}")

# Generate a dal.py shim that re-exports from _dal (pybind11 binding module)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/dal/dal.py
    "# Auto-generated pybind11 shim -- re-exports all symbols from _dal\n"
    "from ._dal import *\n"
)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS _dal
        LIBRARY DESTINATION dal
        RUNTIME DESTINATION dal)

# Install the generated dal.py shim
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dal/dal.py
        DESTINATION dal)

# Install hand-written Python sources from src/dal/
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/dal/
        DESTINATION dal
        FILES_MATCHING PATTERN "*.py"
        PATTERN "__pycache__" EXCLUDE
        PATTERN "*.egg-info" EXCLUDE)

# ---------------------------------------------------------------------------
# Python tests (pytest)
#
# The in-build-tree `dal/` directory contains the compiled `_dal` module
# and the generated `dal.py` shim. The hand-written package files
# (`__init__.py`, `api.py`) live in `src/dal/` and are copied into the
# build tree at configure time so CTest can `import dal` without
# a separate `pip install -e .` step.
# ---------------------------------------------------------------------------
if(DAL_PYTHON_BUILD_TESTS)
    enable_testing()

    file(COPY
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dal/__init__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dal/api.py
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dal)

    add_test(NAME dal_python_pytest
             COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()
