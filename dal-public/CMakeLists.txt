# dal-public: Stable Public C++ API Layer
#
# Wraps dal-cpp internals behind a stable, controlled public API.
# Downstream projects (dal-python, dal-excel) depend only on this layer.
#
# Target: dal_public (alias DAL::public)

cmake_minimum_required(VERSION 3.21.0)

project(dal-public LANGUAGES CXX
    DESCRIPTION "DAL Public C++ API"
    VERSION 1.0.0)

option(DAL_PUBLIC_BUILD_TESTS "Build dal-public tests" ON)

# Need DAL::cpp from the workspace
if(NOT TARGET DAL::cpp)
    find_package(dal-cpp REQUIRED)
endif()

# Workspace root for <dal-public/src/...> and <dal/...> includes
get_filename_component(DAL_WORKSPACE_ROOT "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)

# ---------------------------------------------------------------------------
# Library target: dal_public (alias DAL::public)
#
# Compiles only public/src/* files. Links against DAL::cpp for core symbols.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PUBLIC_SRC_FILES
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.h"
    "${PROJECT_SOURCE_DIR}/auto/*.inc")

add_library(dal_public ${PUBLIC_SRC_FILES})
add_library(DAL::public ALIAS dal_public)

# Both dal-public consumers and itself need workspace root in include path
# so <dal-public/src/...> and <dal-public/auto/...> resolve.
target_include_directories(dal_public PUBLIC ${DAL_WORKSPACE_ROOT})

if(MSVC)
    target_compile_definitions(dal_public PRIVATE IS_BASE)
endif()

target_link_libraries(dal_public PUBLIC DAL::cpp)

set_target_properties(dal_public PROPERTIES
    EXPORT_NAME public
    OUTPUT_NAME dal_public
    VERSION ${PROJECT_VERSION})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION include/dal-public/src
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_public
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DAL_PUBLIC_BUILD_TESTS)
    if(NOT TARGET GTest::gtest_main)
        if(TARGET gtest_main)
            # Sibling googletest target from dal-cpp
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()

    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")

    add_executable(dal_public_tests ${TEST_FILES})
    target_link_libraries(dal_public_tests PRIVATE dal_public)

    if(TARGET gtest_main)
        target_link_libraries(dal_public_tests PRIVATE gtest gtest_main)
    else()
        target_link_libraries(dal_public_tests PRIVATE GTest::gtest_main)
    endif()

    gtest_discover_tests(dal_public_tests)
endif()
