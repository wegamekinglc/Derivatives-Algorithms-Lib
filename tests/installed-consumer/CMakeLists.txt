cmake_minimum_required(VERSION 3.21)

project(dal-installed-consumer LANGUAGES CXX)

find_package(dal-public 1.0 CONFIG REQUIRED)

if(NOT DAL_CPP_AAD_BACKEND STREQUAL "Native")
    message(FATAL_ERROR "dal-cpp package must provide native AAD")
endif()

if(NOT DEFINED DAL_CPP_MSVC_RUNTIME_LIBRARY)
    message(FATAL_ERROR "dal-cpp package did not publish DAL_CPP_MSVC_RUNTIME_LIBRARY")
endif()
if(NOT COMMAND dal_cpp_apply_msvc_runtime)
    message(FATAL_ERROR "dal-cpp package did not publish dal_cpp_apply_msvc_runtime")
endif()

add_executable(dal_installed_consumer main.cpp)
add_executable(dal_installed_aad_consumer native.cpp)
target_compile_definitions(dal_installed_aad_consumer PRIVATE
    EXPECT_DIAGNOSTICS=$<BOOL:${DAL_CPP_AAD_LIFETIME_DIAGNOSTICS}>
    EXPECT_PROFILING=$<BOOL:${DAL_CPP_AAD_PROFILING}>)

foreach(consumer IN ITEMS dal_installed_consumer dal_installed_aad_consumer)
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
