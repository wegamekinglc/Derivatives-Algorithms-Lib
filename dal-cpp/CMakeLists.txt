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

option(DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS "Check native AAD active-number ownership and lifetime" OFF)
option(DAL_ENABLE_AAD_PROFILING "Enable explicit native AAD production profiling" OFF)
option(DAL_USE_EIGEN "Use Eigen for the dense matrix-matrix kernel" ON)

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
# Native AAD and removed configuration migration
# ---------------------------------------------------------------------------
foreach(removed_backend IN ITEMS DAL_USE_XAD_AAD DAL_USE_CODIPACK_AAD DAL_USE_ADEPT_AAD)
    if(${removed_backend})
        message(FATAL_ERROR "${removed_backend} is no longer supported. Reconfigure and rebuild with native AAD; remove this option or set it to OFF.")
    endif()
endforeach()
message(STATUS "USE AAD framework: AADET (native)")

if(DAL_USE_EIGEN)
    message(STATUS "USE dense matrix product: Eigen (externals/eigen)")
else()
    message(STATUS "USE dense matrix product: built-in SIMD kernel")
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

# rapidjson headers are on the dal_cpp private include path.
dal_require_submodule("externals/rapidjson/include/rapidjson/document.h")

# Eigen headers back the dense GEMM dispatch; the hand-rolled slab kernel remains the fallback.
if(DAL_USE_EIGEN)
    dal_require_submodule("externals/eigen/Eigen/Dense")
endif()

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

foreach(diagnostic_option IN ITEMS DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS DAL_ENABLE_AAD_PROFILING)
    if(${diagnostic_option})
        target_compile_definitions(dal_cpp PUBLIC ${diagnostic_option})
    endif()
endforeach()

# Make <dal/...> available to consumers (dal-public, etc.)
target_include_directories(dal_cpp
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        ${PROJECT_SOURCE_DIR}/externals/rapidjson/include)

dal_apply_platform_options(dal_cpp)

if(DAL_USE_EIGEN)
    target_include_directories(dal_cpp PRIVATE ${PROJECT_SOURCE_DIR}/externals/eigen)
    target_compile_definitions(dal_cpp PRIVATE DAL_USE_EIGEN EIGEN_DONT_PARALLELIZE)
endif()

if(MSVC)
    target_compile_definitions(dal_cpp PRIVATE IS_BASE)
endif()

set(DAL_CPP_AAD_BACKEND "Native")

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
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
        PATTERN "eigenbridge.hpp" EXCLUDE)

install(TARGETS dal_cpp
        EXPORT DALCppTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(DAL_CPP_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/dal-cpp")

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

    set(DAL_CPP_BCG_WORKSPACE_BOUNDARY_TEST_FILTER
        "MatrixTest.TestCGSolveAndBCGSolveProductionExactWorkspaceConstructionBoundary")
    set(DAL_CPP_BCG_WORKSPACE_BOUNDARY_TEST_SOURCES
        "${PROJECT_SOURCE_DIR}/dal/math/matrix/bcg.cpp"
        "${PROJECT_SOURCE_DIR}/tests/math/matrix/test_bcg.cpp"
        "${PROJECT_SOURCE_DIR}/test-support/bcg_allocation_probe.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test_main.cpp")

    add_executable(dal_cpp_tests ${TEST_FILES})
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i.86)$")
        # Consumers with different ISA flags must share the library's reduction order.
        target_sources(dal_cpp_tests PRIVATE
            "${PROJECT_SOURCE_DIR}/test-support/simdkernels_sse2.cpp"
            "${PROJECT_SOURCE_DIR}/test-support/simdkernels_avx2.cpp")
        set_source_files_properties("${PROJECT_SOURCE_DIR}/test-support/simdkernels_sse2.cpp"
            PROPERTIES COMPILE_OPTIONS "-msse2;-mno-avx;-mno-avx2;-mno-fma")
        set_source_files_properties("${PROJECT_SOURCE_DIR}/test-support/simdkernels_avx2.cpp"
            PROPERTIES COMPILE_OPTIONS "-mavx2")
        target_compile_definitions(dal_cpp_tests PRIVATE DAL_TEST_MIXED_SIMD)
    endif()
    target_link_libraries(dal_cpp_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)
    target_include_directories(dal_cpp_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/test-support"
        "${PROJECT_SOURCE_DIR}/externals/rapidjson/include")
    dal_apply_platform_options(dal_cpp_tests)

    gtest_discover_tests(dal_cpp_tests)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU" AND NOT MSVC AND NOT CMAKE_CROSSCOMPILING)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
            set(DAL_DUPIRE_REPLAY_FMA_SUPPORTED TRUE)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i.86)$")
            include(CheckCXXSourceRuns)
            check_cxx_source_runs(
                "int main() { return __builtin_cpu_supports(\"fma\") ? 0 : 1; }"
                DAL_DUPIRE_REPLAY_FMA_SUPPORTED)
        endif()
        if(DAL_DUPIRE_REPLAY_FMA_SUPPORTED)
            # Reproduce scalar/active contraction differences without changing library flags.
            add_executable(dal_cpp_dupire_replay_rounding_tests
                "${PROJECT_SOURCE_DIR}/dal/model/dupirerisk.cpp"
                "${PROJECT_SOURCE_DIR}/tests/model/test_dupire_risk.cpp")
            target_link_libraries(dal_cpp_dupire_replay_rounding_tests PRIVATE dal_cpp gtest_main)
            dal_apply_platform_options(dal_cpp_dupire_replay_rounding_tests)
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(dal_cpp_dupire_replay_rounding_tests PRIVATE -ffp-contract=on)
            endif()
            if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|i.86)$")
                target_compile_options(dal_cpp_dupire_replay_rounding_tests PRIVATE -mfma)
            endif()
            gtest_discover_tests(dal_cpp_dupire_replay_rounding_tests TEST_PREFIX "dupire_rounding::")
        endif()
    endif()

    # Keep the global allocation probe out of the ordinary test executable.
    add_executable(dal_cpp_pwl_allocation_tests
        "${PROJECT_SOURCE_DIR}/test-support/test_pwl_allocations.cpp"
        "${PROJECT_SOURCE_DIR}/test-support/bcg_allocation_probe.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test_main.cpp")
    target_link_libraries(dal_cpp_pwl_allocation_tests PRIVATE dal_cpp gtest)
    dal_apply_platform_options(dal_cpp_pwl_allocation_tests)
    gtest_discover_tests(dal_cpp_pwl_allocation_tests TEST_PREFIX "pwl_allocation::")

    add_executable(dal_cpp_schedule_allocation_tests
        "${PROJECT_SOURCE_DIR}/test-support/test_schedule_allocations.cpp"
        "${PROJECT_SOURCE_DIR}/test-support/bcg_allocation_probe.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test_main.cpp")
    target_link_libraries(dal_cpp_schedule_allocation_tests PRIVATE dal_cpp gtest)
    dal_apply_platform_options(dal_cpp_schedule_allocation_tests)
    gtest_discover_tests(dal_cpp_schedule_allocation_tests TEST_PREFIX "schedule_allocation::")

    add_executable(dal_cpp_script_observation_allocation_tests
        "${PROJECT_SOURCE_DIR}/test-support/test_script_observation_allocations.cpp"
        "${PROJECT_SOURCE_DIR}/test-support/bcg_allocation_probe.cpp"
        "${PROJECT_SOURCE_DIR}/tests/test_main.cpp")
    target_link_libraries(dal_cpp_script_observation_allocation_tests PRIVATE dal_cpp gtest)
    target_include_directories(dal_cpp_script_observation_allocation_tests PRIVATE "${PROJECT_SOURCE_DIR}/test-support")
    dal_apply_platform_options(dal_cpp_script_observation_allocation_tests)
    gtest_discover_tests(dal_cpp_script_observation_allocation_tests TEST_PREFIX "script_observation_allocation::")

    add_executable(dal_cpp_bcg_workspace_boundary_tests
        ${DAL_CPP_BCG_WORKSPACE_BOUNDARY_TEST_SOURCES})
    target_compile_definitions(dal_cpp_bcg_workspace_boundary_tests PRIVATE DAL_BCG_WORKSPACE_BOUNDARY_TEST_SEAM)
    target_link_libraries(dal_cpp_bcg_workspace_boundary_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)
    dal_apply_platform_options(dal_cpp_bcg_workspace_boundary_tests)
    if(MSVC)
        target_compile_definitions(dal_cpp_bcg_workspace_boundary_tests PRIVATE IS_BASE)
    endif()

    add_test(
        NAME "bcg_workspace_boundary::${DAL_CPP_BCG_WORKSPACE_BOUNDARY_TEST_FILTER}"
        COMMAND dal_cpp_bcg_workspace_boundary_tests
                "--gtest_filter=${DAL_CPP_BCG_WORKSPACE_BOUNDARY_TEST_FILTER}"
                "--gtest_fail_if_no_test_selected")

    add_test(
        NAME "bcg_allocation_boundary::MatrixTest.TestCGSolveAndBCGSolveScaledAlphaAddsNoHeapAllocations"
        COMMAND dal_cpp_bcg_workspace_boundary_tests
                "--gtest_filter=MatrixTest.TestCGSolveAndBCGSolveScaledAlphaAddsNoHeapAllocations"
                "--gtest_fail_if_no_test_selected")

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
