cmake_minimum_required(VERSION 3.21)

project(DalInstalledBenchmarkRuntime LANGUAGES CXX)
find_package(dal-public 1.0 CONFIG REQUIRED)

if(NOT MSVC)
    # Exercise the exported helper's configuration branch, not a Windows build.
    set(MSVC TRUE)
    set(DAL_TEST_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING
        "SDK runtime contract simulated on non-MSVC hosts")
    set(DAL_CPP_MSVC_RUNTIME_LIBRARY "${DAL_TEST_MSVC_RUNTIME_LIBRARY}")
    message(STATUS "Configure-only MSVC runtime simulation: ${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
endif()

# The SDK contract must override the usual executable default.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../dal-public/benchmarks/compounded_ois_audit_perf"
    "${CMAKE_CURRENT_BINARY_DIR}/audit")
get_target_property(benchmark_runtime compounded_ois_audit_perf MSVC_RUNTIME_LIBRARY)
if(NOT "${benchmark_runtime}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
    message(FATAL_ERROR
        "Audit benchmark runtime '${benchmark_runtime}' differs from SDK '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
endif()
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/runtime-$<CONFIG>.txt"
    CONTENT "$<GENEX_EVAL:$<TARGET_PROPERTY:compounded_ois_audit_perf,MSVC_RUNTIME_LIBRARY>>\n")
