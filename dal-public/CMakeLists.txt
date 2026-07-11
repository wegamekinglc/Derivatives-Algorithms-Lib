# dal-public: Developer-facing C++ convenience facade
#
# Collects common construction, calibration, valuation, and repository entry
# points over dal-cpp. Its headers expose core DAL types and do not provide an
# ABI-isolated boundary; downstream bindings consume both this facade and those
# exposed core contracts.
#
# Target: dal_public (alias DAL::public)

cmake_minimum_required(VERSION 3.21.0)

project(dal-public LANGUAGES CXX
    DESCRIPTION "DAL Public C++ API"
    VERSION 1.0.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(DAL_PUBLIC_BUILD_TESTS "Build dal-public tests" ON)

# Need DAL::cpp from the workspace
set(DAL_PUBLIC_USES_INSTALLED_CPP FALSE)
if(NOT TARGET DAL::cpp)
    find_package(dal-cpp CONFIG REQUIRED)
    set(DAL_PUBLIC_USES_INSTALLED_CPP TRUE)
endif()

# Workspace root for <dal-public/src/...> and <dal/...> includes
get_filename_component(DAL_WORKSPACE_ROOT "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)

# ---------------------------------------------------------------------------
# Library target: dal_public (alias DAL::public)
#
# Compiles only public/src/* files. Links against DAL::cpp for core symbols.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE PUBLIC_SRC_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.h")

add_library(dal_public ${PUBLIC_SRC_FILES})
add_library(DAL::public ALIAS dal_public)

if(DAL_PUBLIC_USES_INSTALLED_CPP)
    if(NOT COMMAND dal_cpp_apply_msvc_runtime)
        message(FATAL_ERROR "installed dal-cpp package did not publish dal_cpp_apply_msvc_runtime")
    endif()
    dal_cpp_apply_msvc_runtime(dal_public)
endif()

# Both dal-public consumers and itself need workspace root in include path
# so <dal-public/src/...> and <dal-cpp/...> resolve.
target_include_directories(dal_public PUBLIC
    "$<BUILD_INTERFACE:${DAL_WORKSPACE_ROOT}>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(dal_public PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(dal_public PRIVATE /utf-8)
    target_compile_definitions(dal_public PRIVATE IS_BASE)
endif()

target_link_libraries(dal_public PUBLIC DAL::cpp)

set_target_properties(dal_public PROPERTIES
    EXPORT_NAME public
    OUTPUT_NAME dal_public
    VERSION ${PROJECT_VERSION})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dal-public/src
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_public
        EXPORT DALPublicTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(DAL_PUBLIC_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/dal-public")
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dal-publicConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dal-publicConfig.cmake
    INSTALL_DESTINATION ${DAL_PUBLIC_CONFIG_INSTALL_DIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/dal-publicConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT DALPublicTargets
        FILE dal-publicTargets.cmake
        NAMESPACE DAL::
        DESTINATION ${DAL_PUBLIC_CONFIG_INSTALL_DIR})
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/dal-publicConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/dal-publicConfigVersion.cmake
        DESTINATION ${DAL_PUBLIC_CONFIG_INSTALL_DIR})

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DAL_PUBLIC_BUILD_TESTS)
    if(PROJECT_IS_TOP_LEVEL)
        enable_testing()
    endif()

    if(NOT TARGET GTest::gtest_main)
        if(TARGET gtest_main)
            # Sibling googletest target from dal-cpp
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()

    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")

    add_executable(dal_public_tests ${TEST_FILES})
    if(DAL_PUBLIC_USES_INSTALLED_CPP)
        dal_cpp_apply_msvc_runtime(dal_public_tests)
        if(MSVC)
            get_target_property(DAL_PUBLIC_TESTS_MSVC_RUNTIME dal_public_tests MSVC_RUNTIME_LIBRARY)
            if(NOT "${DAL_PUBLIC_TESTS_MSVC_RUNTIME}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
                message(FATAL_ERROR
                    "dal_public_tests runtime '${DAL_PUBLIC_TESTS_MSVC_RUNTIME}' does not match "
                    "installed DAL runtime '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
            endif()
        endif()
    endif()
    target_link_libraries(dal_public_tests PRIVATE dal_public)

    if(TARGET gtest_main)
        target_link_libraries(dal_public_tests PRIVATE gtest gtest_main)
    else()
        target_link_libraries(dal_public_tests PRIVATE GTest::gtest_main)
    endif()

    gtest_discover_tests(dal_public_tests)
endif()
