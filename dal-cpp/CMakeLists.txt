# dal-cpp: Core C++ Quant Library
#
# Always built as a sub-project of the DAL workspace.
# Target: dal_cpp (alias DAL::cpp)

cmake_minimum_required(VERSION 3.21.0)

project(dal-cpp LANGUAGES CXX
    DESCRIPTION "DAL Core C++ Quant Library"
    VERSION 1.0.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(DAL_CPP_BUILD_TESTS "Build dal-cpp tests" ON)
option(DAL_CPP_BUILD_EXAMPLES "Build dal-cpp examples" ON)
option(DAL_CPP_BUILD_BENCHMARKS "Build dal-cpp benchmarks" ON)
option(DAL_ENABLE_NATIVE_ARCH "Optimize DAL for the build machine's CPU" OFF)

option(DAL_USE_XAD_AAD "Use XAD AAD backend" OFF)
option(DAL_USE_CODIPACK_AAD "Use CoDiPack AAD backend" OFF)
option(DAL_USE_ADEPT_AAD "Use Adept AAD backend" OFF)

# ---------------------------------------------------------------------------
# C++ standard, PIC, build settings
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS FALSE)

if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if(CMAKE_POSITION_INDEPENDENT_CODE)
    include(CheckPIESupported)
    check_pie_supported()
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${UNIX})
endif()

# Platform-specific compiler flags
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(Platform)

# ---------------------------------------------------------------------------
# AAD backend selection
# ---------------------------------------------------------------------------
if((DAL_USE_XAD_AAD AND DAL_USE_CODIPACK_AAD) OR
   (DAL_USE_XAD_AAD AND DAL_USE_ADEPT_AAD) OR
   (DAL_USE_CODIPACK_AAD AND DAL_USE_ADEPT_AAD))
    message(FATAL_ERROR "Only one external AAD backend can be enabled")
endif()

if(DAL_USE_XAD_AAD)
    message(STATUS "USE AAD framework: XAD")
elseif(DAL_USE_CODIPACK_AAD)
    message(STATUS "USE AAD framework: CoDiPack")
elseif(DAL_USE_ADEPT_AAD)
    message(STATUS "USE AAD framework: Adept")
else()
    message(STATUS "USE AAD framework: AADET (native)")
endif()

# ---------------------------------------------------------------------------
# External dependencies
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_TESTS)
    dal_require_submodule("externals/googletest/CMakeLists.txt")
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory(${PROJECT_SOURCE_DIR}/externals/googletest)
endif()

if(DAL_USE_XAD_AAD OR DAL_CPP_BUILD_EXAMPLES)
    dal_require_submodule("externals/xad/CMakeLists.txt")
endif()
if(DAL_USE_XAD_AAD)
    add_subdirectory(${PROJECT_SOURCE_DIR}/externals/xad)
elseif(DAL_CPP_BUILD_EXAMPLES)
    # The AAD comparison example exercises XAD explicitly even when DAL itself
    # uses another backend. Keep it buildable without exporting or installing XAD.
    add_subdirectory(${PROJECT_SOURCE_DIR}/externals/xad EXCLUDE_FROM_ALL)
endif()

if(DAL_USE_CODIPACK_AAD)
    dal_require_submodule("externals/CodiPack/CMakeLists.txt")
    add_subdirectory(${PROJECT_SOURCE_DIR}/externals/CodiPack)
elseif(DAL_USE_ADEPT_AAD)
    dal_require_submodule("externals/adept/CMakeLists.txt")
    add_subdirectory(${PROJECT_SOURCE_DIR}/externals/adept)
endif()

# rapidjson headers are on the dal_cpp private include path.
dal_require_submodule("externals/rapidjson/include/rapidjson/document.h")

# Machinist is an explicit source-generation tool. It is built only when
# dal_generate/dal_check_generated is requested and is never part of DAL installs.
dal_require_submodule("externals/machinist/CMakeLists.txt")
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/machinist EXCLUDE_FROM_ALL)

add_custom_target(dal_generate
    COMMAND ${CMAKE_COMMAND} -E env
            "MACHINIST_TEMPLATE_DIR=${PROJECT_SOURCE_DIR}/externals/machinist/template/"
            $<TARGET_FILE:Machinist>
            -c ${PROJECT_SOURCE_DIR}/config/dal.ifc
            -l ${PROJECT_SOURCE_DIR}/config/dal.mgl
            -d ${PROJECT_SOURCE_DIR}/dal
    COMMAND ${CMAKE_COMMAND}
            -DDAL_REPOSITORY_ROOT=${PROJECT_SOURCE_DIR}/..
            -P ${PROJECT_SOURCE_DIR}/cmake/normalize-calibration-generated-enums.cmake
    COMMAND ${CMAKE_COMMAND} -E env
            "MACHINIST_TEMPLATE_DIR=${PROJECT_SOURCE_DIR}/externals/machinist/template/"
            $<TARGET_FILE:Machinist>
            -c ${PROJECT_SOURCE_DIR}/config/dal.ifc
            -l ${PROJECT_SOURCE_DIR}/config/dal.mgl
            -d ${PROJECT_SOURCE_DIR}/../dal-excel
    DEPENDS Machinist
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/..
    COMMENT "Regenerating DAL core and Excel sources with Machinist"
    VERBATIM)

find_package(Git QUIET)
if(Git_FOUND)
    add_custom_target(dal_check_generated
        COMMAND ${CMAKE_COMMAND}
                -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
                -DDAL_REPOSITORY_ROOT=${PROJECT_SOURCE_DIR}/..
                -P ${PROJECT_SOURCE_DIR}/cmake/check-generated.cmake
        DEPENDS dal_generate
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/..
        COMMENT "Checking generated DAL sources for drift"
        VERBATIM)
endif()

# ---------------------------------------------------------------------------
# Library target: dal_cpp (alias DAL::cpp)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE DAL_CPP_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/dal/*.h"
    "${PROJECT_SOURCE_DIR}/dal/*.hpp"
    "${PROJECT_SOURCE_DIR}/dal/*.cpp"
    "${PROJECT_SOURCE_DIR}/dal/*.inc")
list(FILTER DAL_CPP_SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/storage/_repository.*")

add_library(dal_cpp ${DAL_CPP_SOURCES})
add_library(DAL::cpp ALIAS dal_cpp)

# Propagate C++17 to consumers (dal-public, examples, tests).
target_compile_features(dal_cpp PUBLIC cxx_std_17)

# Make <dal/...> available to consumers (dal-public, etc.)
target_include_directories(dal_cpp
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        ${PROJECT_SOURCE_DIR}/externals/rapidjson/include)

dal_apply_platform_options(dal_cpp)

if(MSVC)
    target_compile_definitions(dal_cpp PRIVATE IS_BASE)
endif()

if(DAL_USE_XAD_AAD)
    set(DAL_CPP_AAD_BACKEND "XAD")
    target_link_libraries(dal_cpp PUBLIC
        "$<BUILD_INTERFACE:XAD::xad>"
        "$<INSTALL_INTERFACE:XAD::xad>")
    target_compile_definitions(dal_cpp PUBLIC DAL_USE_XAD_AAD)
elseif(DAL_USE_CODIPACK_AAD)
    set(DAL_CPP_AAD_BACKEND "CoDiPack")
    target_link_libraries(dal_cpp PUBLIC
        "$<BUILD_INTERFACE:CoDiPack>"
        "$<INSTALL_INTERFACE:DAL::CoDiPack>")
    target_compile_definitions(dal_cpp PUBLIC DAL_USE_CODIPACK_AAD CODI_ForcedInlines)
elseif(DAL_USE_ADEPT_AAD)
    set(DAL_CPP_AAD_BACKEND "Adept")
    target_link_libraries(dal_cpp PUBLIC
        "$<BUILD_INTERFACE:adept>"
        "$<INSTALL_INTERFACE:DAL::adept>")
    target_compile_definitions(dal_cpp PUBLIC
        DAL_USE_ADEPT_AAD
        ADEPT_FAST_EXPONENTIAL
        ADEPT_NO_DIMENSION_CHECKING
        ADEPT_NO_ALIAS_CHECKING
        ADEPT_INITIAL_STACK_LENGTH=10485760)
    target_include_directories(dal_cpp PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/externals/adept/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
else()
    set(DAL_CPP_AAD_BACKEND "Native")
endif()

set_target_properties(dal_cpp PROPERTIES
    EXPORT_NAME cpp
    OUTPUT_NAME dal_cpp
    VERSION ${PROJECT_VERSION})

# Capture the runtime ABI from the target that is actually installed. The
# configured package republishes this value so MSVC consumers can match it.
set(DAL_CPP_MSVC_RUNTIME_LIBRARY "")
if(MSVC)
    get_target_property(DAL_CPP_MSVC_RUNTIME_LIBRARY dal_cpp MSVC_RUNTIME_LIBRARY)
    if(NOT DAL_CPP_MSVC_RUNTIME_LIBRARY)
        message(FATAL_ERROR "dal_cpp target does not declare an MSVC runtime library")
    endif()
endif()

install(DIRECTORY ${PROJECT_SOURCE_DIR}/dal/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_cpp
        EXPORT DALCppTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(DAL_CPP_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/dal-cpp")

if(DAL_USE_CODIPACK_AAD)
    install(TARGETS CoDiPack EXPORT DALCoDiPackTargets)
    install(EXPORT DALCoDiPackTargets
            FILE DALCoDiPackTargets.cmake
            NAMESPACE DAL::
            DESTINATION ${DAL_CPP_CONFIG_INSTALL_DIR})
elseif(DAL_USE_ADEPT_AAD)
    install(TARGETS adept
            EXPORT DALAdeptTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(EXPORT DALAdeptTargets
            FILE DALAdeptTargets.cmake
            NAMESPACE DAL::
            DESTINATION ${DAL_CPP_CONFIG_INSTALL_DIR})
endif()

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/dal-cppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/dal-cppConfig.cmake
    INSTALL_DESTINATION ${DAL_CPP_CONFIG_INSTALL_DIR})
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/dal-cppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(EXPORT DALCppTargets
        FILE dal-cppTargets.cmake
        NAMESPACE DAL::
        DESTINATION ${DAL_CPP_CONFIG_INSTALL_DIR})
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/dal-cppConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/dal-cppConfigVersion.cmake
        DESTINATION ${DAL_CPP_CONFIG_INSTALL_DIR})

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_TESTS)
    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")

    set(DAL_CPP_DAL35_SEAM_TEST_FILTER
        "MatrixTest.TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary")
    set(DAL_CPP_DAL35_SEAM_BASE_SOURCES
        "${PROJECT_SOURCE_DIR}/dal/math/matrix/bcg.cpp"
        "${PROJECT_SOURCE_DIR}/tests/math/matrix/test_bcg.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test_main.cpp")

    # BEGIN DAL58_P4_PRIVATE_TARGET_SOURCES
    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)
    # END DAL58_P4_PRIVATE_TARGET_SOURCES

    set(DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE)
    foreach(private_source IN LISTS DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES)
        cmake_path(
            ABSOLUTE_PATH private_source
            BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
            NORMALIZE
            OUTPUT_VARIABLE private_source_absolute)
        list(APPEND DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE
            "${private_source_absolute}")
    endforeach()
    list(REMOVE_ITEM TEST_FILES
        ${DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES_ABSOLUTE})

    add_executable(dal_cpp_tests ${TEST_FILES})
    target_link_libraries(dal_cpp_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)
    dal_apply_platform_options(dal_cpp_tests)

    gtest_discover_tests(dal_cpp_tests)

    add_executable(dal_cpp_dal35_seam_tests
        ${DAL_CPP_DAL35_SEAM_BASE_SOURCES}
        ${DAL_CPP_DAL35_P4_PRIVATE_TARGET_SOURCES})
    target_compile_definitions(dal_cpp_dal35_seam_tests PRIVATE DAL35_ENABLE_TEST_SEAM)
    target_link_libraries(dal_cpp_dal35_seam_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)
    dal_apply_platform_options(dal_cpp_dal35_seam_tests)
    if(MSVC)
        target_compile_definitions(dal_cpp_dal35_seam_tests PRIVATE IS_BASE)
    endif()

    function(_dal_cpp_register_dal35_seam_test test_filter)
        add_test(
            NAME "dal35_seam::${test_filter}"
            COMMAND dal_cpp_dal35_seam_tests
                    "--gtest_filter=${test_filter}"
                    "--gtest_fail_if_no_test_selected")
    endfunction()

    # BEGIN DAL58_P4_PRIVATE_TARGET_CTEST
    _dal_cpp_register_dal35_seam_test("${DAL_CPP_DAL35_SEAM_TEST_FILTER}")
    # END DAL58_P4_PRIVATE_TARGET_CTEST

    add_test(
        NAME "dal35_scope_guard::fixtures"
        COMMAND ${CMAKE_COMMAND}
                -DDAL35_GUARD_SCRIPT=${PROJECT_SOURCE_DIR}/cmake/dal35-private-target-scope-guard.cmake
                -P ${PROJECT_SOURCE_DIR}/cmake/test-dal35-private-target-scope-guard.cmake)

    install(TARGETS dal_cpp_tests
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE)
endif()

# ---------------------------------------------------------------------------
# Examples and benchmarks
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_EXAMPLES OR DAL_CPP_BUILD_BENCHMARKS)
    # Examples and benchmarks reference dal_library - provide compatibility alias
    if(NOT TARGET dal_library)
        add_library(dal_library ALIAS dal_cpp)
    endif()
endif()

if(DAL_CPP_BUILD_EXAMPLES)
    add_subdirectory(${PROJECT_SOURCE_DIR}/examples)
endif()

if(DAL_CPP_BUILD_BENCHMARKS)
    add_subdirectory(${PROJECT_SOURCE_DIR}/benchmarks)
endif()
