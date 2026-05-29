# dal-public: Stable Public C++ API Layer
#
# This project wraps dal-cpp internals behind a stable, controlled public API.
# Downstream projects (dal-python, dal-excel) depend only on this layer.
#
# Target: dal_public (alias DAL::public)
#
# Key fix: Unlike the old public/src/CMakeLists.txt, dal_public NEVER compiles
# dal/*.cpp files itself. It always links against DAL::cpp / dal_library.
# This eliminates the duplicate compilation in static builds.

cmake_minimum_required(VERSION 3.21.0)

# ---------------------------------------------------------------------------
# Detect whether we are the top-level project or a subdirectory.
# The parent CMakeLists.txt sets DAL_PUBLIC_AS_SUBDIRECTORY=TRUE before
# calling add_subdirectory(dal-public). If not set, we're standalone.
# ---------------------------------------------------------------------------
if(NOT DAL_PUBLIC_AS_SUBDIRECTORY)
    # Top-level: called via cmake -S dal-public -B build
    set(DAL_PUBLIC_IS_TOPLEVEL TRUE)
else()
    # Subdirectory: called via add_subdirectory(dal-public) from parent
    set(DAL_PUBLIC_IS_TOPLEVEL FALSE)
endif()

# ---------------------------------------------------------------------------
# Project-level setup (only when top-level)
# ---------------------------------------------------------------------------
if(DAL_PUBLIC_IS_TOPLEVEL)
    set(DAL_VERSION_MAJOR 1)
    set(DAL_VERSION_MINOR 0)
    set(DAL_VERSION_PATCH 0)
    set(DAL_VERSION ${DAL_VERSION_MAJOR}.${DAL_VERSION_MINOR}.${DAL_VERSION_PATCH})

    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    endif()

    # project() must come BEFORE we check CMAKE_SOURCE_DIR for subdirs
    # Since this is the top-level, calling project() is safe here
endif()

# Only call project() if we're top-level
if(DAL_PUBLIC_IS_TOPLEVEL)
    project(dal-public LANGUAGES CXX
        DESCRIPTION "DAL Public C++ API"
        VERSION ${DAL_VERSION})

    message(STATUS "dal-public: standalone top-level mode")
    message(STATUS "-- Build Mode: ${CMAKE_BUILD_TYPE}")

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS FALSE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()

    if(NOT DEFINED BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${UNIX})
    endif()
    message(STATUS "-- BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

    # Installation directories
    set(DAL_INSTALL_LIBDIR "lib" CACHE STRING "Installation directory for libraries")
    set(DAL_INSTALL_INCLUDEDIR "include" CACHE STRING "Installation directory for headers")
    set(DAL_INSTALL_CMAKEDIR "lib/cmake/dal-public" CACHE STRING "Installation directory for CMake scripts")
else()
    message(STATUS "dal-public: subdirectory mode (added via add_subdirectory)")
endif()

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_PUBLIC_BUILD_TESTS "Build dal-public tests" ON)

# ---------------------------------------------------------------------------
# Find the core DAL library
#
# Priority: 1) sibling DAL::cpp (dal-cpp refactored target),
#           2) sibling dal_library (old build target),
#           3) installed dal-cpp package
# ---------------------------------------------------------------------------
if(TARGET DAL::cpp)
    message(STATUS "  core: linking against sibling DAL::cpp")
    set(DAL_CORE_TARGET DAL::cpp)
elseif(TARGET dal_library)
    message(STATUS "  core: linking against sibling dal_library (old build)")
    set(DAL_CORE_TARGET dal_library)
else()
    if(DAL_PUBLIC_IS_TOPLEVEL)
        message(STATUS "  core: searching for installed dal-cpp package")
        find_package(dal-cpp REQUIRED)
        if(TARGET DAL::cpp)
            set(DAL_CORE_TARGET DAL::cpp)
        else()
            message(FATAL_ERROR "dal-public requires DAL::cpp target from dal-cpp package")
        endif()
    else()
        message(FATAL_ERROR "dal-public requires DAL::cpp or dal_library target to be defined before add_subdirectory(dal-public)")
    endif()
endif()

# ---------------------------------------------------------------------------
# Include paths
# In subdirectory mode the parent already sets include_directories to the
# repo root, so <public/src/...> and <dal/...> work.
# In top-level mode we need the repo root via the parent directory.
# ---------------------------------------------------------------------------
if(DAL_PUBLIC_IS_TOPLEVEL)
    include_directories(${PROJECT_SOURCE_DIR}/..)
endif()

# ---------------------------------------------------------------------------
# Library target: dal_public (alias DAL::public)
#
# CRITICAL: Only compile public/src/* files. Do NOT compile dal/*.cpp.
# This fixes the original duplicate compilation bug in public/src/CMakeLists.txt
# where static builds compiled all core sources into dal_public again.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PUBLIC_SRC_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/auto/*.inc")

add_library(dal_public ${PUBLIC_SRC_FILES})
add_library(DAL::public ALIAS dal_public)

# MSVC export define (mirrors old public/src/CMakeLists.txt)
if(MSVC)
    target_compile_definitions(dal_public PRIVATE IS_BASE)
endif()

# Link against core DAL library (DAL::cpp or dal_library)
target_link_libraries(dal_public PUBLIC ${DAL_CORE_TARGET})

# Target properties
set_target_properties(dal_public PROPERTIES
    EXPORT_NAME public
    OUTPUT_NAME dal_public)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/src/ DESTINATION include/dal_public
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_public
        EXPORT dal-publicTargets
        ARCHIVE DESTINATION ${DAL_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${DAL_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DAL_INSTALL_LIBDIR})

# ---------------------------------------------------------------------------
# Test target: dal_public_tests
# ---------------------------------------------------------------------------
if(DAL_PUBLIC_BUILD_TESTS)
    enable_testing()

    if(NOT TARGET GTest::gtest_main)
        find_package(GTest REQUIRED)
    endif()

    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

    add_executable(dal_public_tests ${TEST_FILES})
    target_link_libraries(dal_public_tests PRIVATE dal_public)
    target_link_libraries(dal_public_tests PRIVATE GTest::gtest_main)

    gtest_discover_tests(dal_public_tests)
endif()
