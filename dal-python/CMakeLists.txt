# dal-python: Python Bindings via pybind11
#
# This project generates Python bindings for the DAL public API using pybind11.
# It depends ONLY on dal-public (DAL::public), never directly on dal-cpp.
#
# Target: _dal (alias DAL::python)
#
# Two build modes:
#   Standalone:  cmake -S dal-python -B build -DDAL_INSTALL_PREFIX=<dal-install-root>
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
set(DAL_INSTALL_PREFIX "" CACHE PATH "Installed DAL prefix containing dal-publicConfig.cmake")
if(NOT DAL_INSTALL_PREFIX AND DAL_DIR)
    set(DAL_INSTALL_PREFIX "${DAL_DIR}" CACHE PATH "Installed DAL prefix containing dal-publicConfig.cmake" FORCE)
endif()

# ---------------------------------------------------------------------------
# Dependencies: pybind11, Python3, and dal-public
# ---------------------------------------------------------------------------
# Find Python3 FIRST so pybind11's internal FindPythonLibsNew reuses the
# same interpreter and include paths (avoids picking up a system Python
# without dev headers).
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "-- Python3: ${Python3_EXECUTABLE} (version ${Python3_VERSION})")

# Find pybind11: prefer a discoverable system install, then the package in the
# selected Python environment, and otherwise build the pinned v2.11.1 vendored
# under dal-cpp/externals/pybind11 (sibling of this project).
find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" -m pybind11 --cmakedir
        RESULT_VARIABLE DAL_PYBIND11_CMAKEDIR_RESULT
        OUTPUT_VARIABLE DAL_PYBIND11_CMAKEDIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(DAL_PYBIND11_CMAKEDIR_RESULT EQUAL 0 AND EXISTS "${DAL_PYBIND11_CMAKEDIR}/pybind11Config.cmake")
        find_package(pybind11 2.11.1 CONFIG QUIET PATHS "${DAL_PYBIND11_CMAKEDIR}" NO_DEFAULT_PATH)
    endif()
endif()
if(NOT pybind11_FOUND)
    # Resolve the submodule path. In monorepo mode (dal-python pulled in via
    # add_subdirectory from the top-level workspace), the submodule lives at
    # ${CMAKE_SOURCE_DIR}/dal-cpp/externals/pybind11. In standalone mode the
    # cache variable DAL_PYBIND11_DIR lets the caller override the location
    # and defaults to the relative sibling path.
    if(DAL_PYTHON_IS_TOPLEVEL)
        set(_DAL_PYBIND11_DEFAULT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../dal-cpp/externals/pybind11")
    else()
        set(_DAL_PYBIND11_DEFAULT_DIR "${CMAKE_SOURCE_DIR}/dal-cpp/externals/pybind11")
    endif()
    set(DAL_PYBIND11_DIR "${_DAL_PYBIND11_DEFAULT_DIR}" CACHE PATH
        "Path to the vendored pybind11 source tree (dal-cpp/externals/pybind11)")
    if(NOT EXISTS "${DAL_PYBIND11_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "dal-python: pybind11 not found via find_package and the vendored "
            "submodule is missing at '${DAL_PYBIND11_DIR}'. Run "
            "'git submodule update --init dal-cpp/externals/pybind11' or "
            "install pybind11 system-wide, then reconfigure.")
    endif()
    message(STATUS "-- pybind11: not found via find_package, using submodule at ${DAL_PYBIND11_DIR} (v2.11.1)")
    add_subdirectory("${DAL_PYBIND11_DIR}" "${CMAKE_BINARY_DIR}/_pybind11_submodule")
else()
    message(STATUS "-- pybind11: using system pybind11 (${pybind11_VERSION})")
endif()

# Find the DAL public library
set(DAL_PYTHON_USES_INSTALLED_PUBLIC FALSE)
if(TARGET DAL::public)
    message(STATUS "-- core: linking against sibling DAL::public")
    set(DAL_PUBLIC_TARGET DAL::public)
elseif(TARGET dal_public)
    message(STATUS "-- core: linking against sibling dal_public")
    set(DAL_PUBLIC_TARGET dal_public)
else()
    if(DAL_PYTHON_IS_TOPLEVEL)
        if(DAL_INSTALL_PREFIX)
            list(PREPEND CMAKE_PREFIX_PATH "${DAL_INSTALL_PREFIX}")
        endif()
        find_package(dal-public CONFIG REQUIRED)
        if(NOT TARGET DAL::public)
            message(FATAL_ERROR "dal-public package did not define DAL::public")
        endif()
        message(STATUS "-- core: linking against installed DAL::public from ${DAL_INSTALL_PREFIX}")
        set(DAL_PUBLIC_TARGET DAL::public)
        set(DAL_PYTHON_USES_INSTALLED_PUBLIC TRUE)
    else()
        message(FATAL_ERROR "dal-python requires dal-public (DAL::public or dal_public) target")
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/calendar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/curve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/global.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/models.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/random.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/script.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bindings/value.cpp)

if(DAL_PYTHON_USES_INSTALLED_PUBLIC)
    if(NOT COMMAND dal_cpp_apply_msvc_runtime)
        message(FATAL_ERROR "installed dal-cpp package did not publish dal_cpp_apply_msvc_runtime")
    endif()
    dal_cpp_apply_msvc_runtime(_dal)
    if(MSVC)
        get_target_property(DAL_PYTHON_MSVC_RUNTIME _dal MSVC_RUNTIME_LIBRARY)
        if(NOT "${DAL_PYTHON_MSVC_RUNTIME}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
            message(FATAL_ERROR
                "_dal runtime '${DAL_PYTHON_MSVC_RUNTIME}' does not match "
                "installed DAL runtime '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
        endif()
    endif()
endif()

add_library(DAL::python ALIAS _dal)

if(MSVC)
    target_compile_options(_dal PRIVATE /utf-8)
endif()

# Link against the public library
if(DAL_PUBLIC_TARGET)
    target_link_libraries(_dal PRIVATE ${DAL_PUBLIC_TARGET})
endif()

# Set Python module properties
target_include_directories(_dal PRIVATE ${Python3_INCLUDE_DIRS})

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
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dal)
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
#
# For multi-config generators (Visual Studio, Xcode) the compiled .pyd
# lands in a <Config> subdirectory (e.g. dal/Release/).  A POST_BUILD
# step copies it up to dal/ so the Python package files and the native
# module live in the same directory regardless of generator.
# ---------------------------------------------------------------------------
if(DAL_PYTHON_BUILD_TESTS)
    enable_testing()

    file(COPY
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dal/__init__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dal/api.py
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/dal)

    add_custom_command(TARGET _dal POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:_dal>"
            "${CMAKE_CURRENT_BINARY_DIR}/dal/")

    add_test(NAME dal_python_pytest
             COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()
