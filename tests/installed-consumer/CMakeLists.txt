cmake_minimum_required(VERSION 3.21)

project(dal-installed-consumer LANGUAGES CXX)

option(DAL_CONSUMER_CORE_ONLY "Check the core SDK without an installed facade" OFF)
if(DAL_CONSUMER_CORE_ONLY)
    find_package(dal-cpp 1.0 CONFIG REQUIRED)
else()
    find_package(dal-public 1.0 CONFIG REQUIRED)
endif()

if(NOT DAL_CPP_AAD_BACKEND STREQUAL "Native")
    message(FATAL_ERROR "dal-cpp package must provide native AAD")
endif()

if(NOT DEFINED DAL_CPP_MSVC_RUNTIME_LIBRARY)
    message(FATAL_ERROR "dal-cpp package did not publish DAL_CPP_MSVC_RUNTIME_LIBRARY")
endif()
if(NOT COMMAND dal_cpp_apply_msvc_runtime)
    message(FATAL_ERROR "dal-cpp package did not publish dal_cpp_apply_msvc_runtime")
endif()

enable_testing()
add_executable(dal_installed_core_ois compoundedois.cpp)
target_link_libraries(dal_installed_core_ois PRIVATE DAL::cpp)
dal_cpp_apply_msvc_runtime(dal_installed_core_ois)
add_test(NAME dal_installed_core_ois COMMAND dal_installed_core_ois)
if(DAL_CONSUMER_CORE_ONLY)
    return()
endif()

add_executable(dal_installed_public_ois compoundedois.cpp)
target_compile_definitions(dal_installed_public_ois PRIVATE DAL_TEST_PUBLIC_OIS)
target_link_libraries(dal_installed_public_ois PRIVATE DAL::public)
dal_cpp_apply_msvc_runtime(dal_installed_public_ois)
add_test(NAME dal_installed_public_ois COMMAND dal_installed_public_ois)

add_executable(dal_installed_market_iv_local_vol marketivlocalvol.cpp)
target_link_libraries(dal_installed_market_iv_local_vol PRIVATE DAL::public)
dal_cpp_apply_msvc_runtime(dal_installed_market_iv_local_vol)
add_test(NAME dal_installed_market_iv_local_vol COMMAND dal_installed_market_iv_local_vol)

add_executable(dal_installed_market_iv_script_refit marketivscriptrefit.cpp)
target_link_libraries(dal_installed_market_iv_script_refit PRIVATE DAL::public)
dal_cpp_apply_msvc_runtime(dal_installed_market_iv_script_refit)
add_test(NAME dal_installed_market_iv_script_refit COMMAND dal_installed_market_iv_script_refit)

set(benchmark_runtime_command "${CMAKE_COMMAND}"
    -S "${CMAKE_CURRENT_SOURCE_DIR}/benchmark-runtime"
    -B "${CMAKE_CURRENT_BINARY_DIR}/benchmark-runtime"
    -G "${CMAKE_GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-Ddal-public_DIR=${dal-public_DIR}"
    "-Ddal-cpp_DIR=${dal-cpp_DIR}")
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND benchmark_runtime_command -A "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND benchmark_runtime_command -T "${CMAKE_GENERATOR_TOOLSET}")
endif()
add_test(NAME dal_installed_public_benchmark_runtime COMMAND ${benchmark_runtime_command})

add_executable(dal_installed_consumer main.cpp)
add_executable(dal_installed_aad_consumer native.cpp)
add_executable(dal_installed_buffer_consumer buffers.cpp)
add_executable(dal_installed_sobol_consumer sobol.cpp)
add_executable(dal_installed_core_pde_risk pderisk.cpp)
add_executable(dal_installed_public_pde_risk pderisk.cpp)
target_compile_definitions(dal_installed_public_pde_risk PRIVATE DAL_TEST_PUBLIC_PDE_RISK)
target_link_libraries(dal_installed_core_pde_risk PRIVATE DAL::cpp)
target_link_libraries(dal_installed_public_pde_risk PRIVATE DAL::public)
dal_cpp_apply_msvc_runtime(dal_installed_core_pde_risk)
dal_cpp_apply_msvc_runtime(dal_installed_public_pde_risk)
target_compile_definitions(dal_installed_aad_consumer PRIVATE
    EXPECT_DIAGNOSTICS=$<BOOL:${DAL_CPP_AAD_LIFETIME_DIAGNOSTICS}>
    EXPECT_PROFILING=$<BOOL:${DAL_CPP_AAD_PROFILING}>)

foreach(consumer IN ITEMS dal_installed_consumer dal_installed_aad_consumer dal_installed_buffer_consumer dal_installed_sobol_consumer)
    dal_cpp_apply_msvc_runtime(${consumer})
    target_link_libraries(${consumer} PRIVATE DAL::public)
    if(MSVC)
        get_target_property(consumer_runtime ${consumer} MSVC_RUNTIME_LIBRARY)
        if(NOT "${consumer_runtime}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
            message(FATAL_ERROR
                "installed consumer runtime '${consumer_runtime}' does not match DAL runtime '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
        endif()
    endif()
endforeach()

enable_testing()
add_test(NAME dal_installed_consumer COMMAND dal_installed_consumer)
add_test(NAME dal_installed_aad_consumer COMMAND dal_installed_aad_consumer)
add_test(NAME dal_installed_buffer_consumer COMMAND dal_installed_buffer_consumer)
add_test(NAME dal_installed_sobol_consumer COMMAND dal_installed_sobol_consumer)
add_test(NAME dal_installed_core_pde_risk COMMAND dal_installed_core_pde_risk)
add_test(NAME dal_installed_public_pde_risk COMMAND dal_installed_public_pde_risk)
