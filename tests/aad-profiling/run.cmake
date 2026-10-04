cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED DAL_AAD_PROFILING_ROOT)
    get_filename_component(DAL_AAD_PROFILING_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()
if(NOT DEFINED DAL_AAD_PROFILING_BINARY_DIR)
    set(DAL_AAD_PROFILING_BINARY_DIR "${DAL_AAD_PROFILING_ROOT}/build/aad-profiling-configuration")
endif()

function(check_configuration name expected)
    if(DEFINED DAL_AAD_PROFILING_CASE AND NOT DAL_AAD_PROFILING_CASE STREQUAL name)
        return()
    endif()
    set(case_dir "${DAL_AAD_PROFILING_BINARY_DIR}/${name}")
    set(command "${CMAKE_COMMAND}" -S "${DAL_AAD_PROFILING_ROOT}" -B "${case_dir}"
        -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
        -DDAL_CPP_BUILD_TESTS=OFF -DDAL_CPP_BUILD_EXAMPLES=OFF
        -DDAL_CPP_BUILD_BENCHMARKS=OFF -DDAL_BUILD_PUBLIC=OFF
        -DDAL_BUILD_PYTHON=OFF -DDAL_BUILD_EXCEL=OFF
        -DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF ${ARGN})
    if(DEFINED DAL_GENERATOR)
        list(APPEND command -G "${DAL_GENERATOR}")
    endif()
    if(DEFINED DAL_CXX_COMPILER)
        list(APPEND command "-DCMAKE_CXX_COMPILER=${DAL_CXX_COMPILER}")
    endif()
    if(DEFINED DAL_MAKE_PROGRAM)
        list(APPEND command "-DCMAKE_MAKE_PROGRAM=${DAL_MAKE_PROGRAM}")
    endif()
    execute_process(COMMAND ${command} RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 120)
    file(MAKE_DIRECTORY "${case_dir}")
    file(WRITE "${case_dir}/configuration.log" "${output}\n${error}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "${name}: native profiling configuration failed\n${output}\n${error}")
    endif()
    file(READ "${case_dir}/dal-cpp/dal-cppConfig.cmake" config)
    if(NOT config MATCHES "set\\(DAL_CPP_AAD_PROFILING \"${expected}\"\\)")
        message(FATAL_ERROR "${name}: native package does not report expected profiling setting ${expected}")
    endif()
    message(STATUS "${name}: profiling metadata ${expected} passed")
endfunction()

check_configuration(default OFF)
check_configuration(enabled ON -DDAL_ENABLE_AAD_PROFILING=ON)
check_configuration(lifetime-and-profiling ON
    -DDAL_ENABLE_AAD_PROFILING=ON -DDAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS=ON)
