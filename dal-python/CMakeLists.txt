# dal-python: Python Bindings via SWIG
#
# This project generates Python bindings for the DAL public API using SWIG.
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
# Dependencies: SWIG, Python3, and dal-public
# ---------------------------------------------------------------------------
find_package(SWIG REQUIRED)
message(STATUS "-- SWIG version: ${SWIG_VERSION}")

find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "-- Python3: ${Python3_EXECUTABLE} (version ${Python3_VERSION})")

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
                message(WARNING "dal-python: DAL libraries not found in ${DAL_DIR}/lib/")
                message(WARNING "  Expected: ${DAL_PUBLIC_LIB} + ${DAL_CPP_LIB}")
                set(DAL_PUBLIC_TARGET "")
            endif()
        else()
            message(WARNING "dal-python: DAL::public not found and DAL_DIR not set or invalid. Python module may not link correctly.")
            set(DAL_PUBLIC_TARGET "")
        endif()
    else()
        message(FATAL_ERROR "dal-python requires dal-public (DAL::public or dal_public) target")
    endif()
endif()

# ---------------------------------------------------------------------------
# Include paths
# In monorepo mode, the parent already sets include_directories to the repo
# root, so <dal/...> and <dal-public/...> includes work.
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
# Python module: _dal (built via SWIG)
#
# The main SWIG interface is swig/dal.i.
# It includes other .i files and references <dal/...> and <dal-public/src/...>
# headers. These resolve via the repo-root include path (or DAL_DIR/include).
# ---------------------------------------------------------------------------

# UseSWIG module must be included after finding SWIG
include(UseSWIG)

# SWIG settings
set(CMAKE_SWIG_FLAGS "")
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/swig/dal.i PROPERTIES
    CPLUSPLUS ON
    SWIG_FLAGS "-outdir;${CMAKE_CURRENT_BINARY_DIR}/dal")

# Create SWIG Python module
swig_add_library(dal_python
    TYPE MODULE
    LANGUAGE python
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/dal
    OUTFILE_DIR ${CMAKE_CURRENT_BINARY_DIR}
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/swig/dal.i)

add_library(DAL::python ALIAS dal_python)

# Link against the public library
if(DAL_PUBLIC_TARGET)
    target_link_libraries(dal_python PRIVATE ${DAL_PUBLIC_TARGET})
endif()

# Set Python module properties
target_include_directories(dal_python PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(dal_python PRIVATE Python3::Module)

# AAD backend include paths for the SWIG wrapper compilation.
# In monorepo mode, adept headers are at <repo>/include; in standalone
# mode DAL_DIR points to the install root with include/ beneath it.
if(DAL_USE_ADEPT_AAD OR DAL_USE_XAD_AAD)
    if(DAL_DIR)
        target_include_directories(dal_python PRIVATE "${DAL_DIR}/include")
    else()
        target_include_directories(dal_python PRIVATE "${CMAKE_INSTALL_PREFIX}/include")
    endif()
endif()

if(WIN32)
    set(DAL_PYTHON_MODULE_SUFFIX ".${Python3_SOABI}.pyd")
else()
    set(DAL_PYTHON_MODULE_SUFFIX ".${Python3_SOABI}${CMAKE_SHARED_MODULE_SUFFIX}")
endif()

set_target_properties(dal_python PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/dal
    PREFIX ""
    OUTPUT_NAME _dal
    SUFFIX "${DAL_PYTHON_MODULE_SUFFIX}")

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS dal_python
        LIBRARY DESTINATION dal
        RUNTIME DESTINATION dal)

# Install SWIG-generated dal.py from the build directory
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
# ---------------------------------------------------------------------------
if(DAL_PYTHON_BUILD_TESTS)
    enable_testing()

    add_test(NAME dal_python_pytest
             COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endif()
