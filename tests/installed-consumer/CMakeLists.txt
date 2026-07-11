cmake_minimum_required(VERSION 3.21)

project(dal-installed-consumer LANGUAGES CXX)

find_package(dal-cpp 1.0 CONFIG REQUIRED)
find_package(dal-public 1.0 CONFIG REQUIRED)

if(NOT DEFINED DAL_CPP_MSVC_RUNTIME_LIBRARY)
    message(FATAL_ERROR "dal-cpp package did not publish DAL_CPP_MSVC_RUNTIME_LIBRARY")
endif()
if(NOT COMMAND dal_cpp_apply_msvc_runtime)
    message(FATAL_ERROR "dal-cpp package did not publish dal_cpp_apply_msvc_runtime")
endif()

add_executable(dal_installed_consumer main.cpp)
dal_cpp_apply_msvc_runtime(dal_installed_consumer)
target_link_libraries(dal_installed_consumer PRIVATE DAL::cpp DAL::public)

if(MSVC)
    get_target_property(consumer_runtime dal_installed_consumer MSVC_RUNTIME_LIBRARY)
    if(NOT "${consumer_runtime}" STREQUAL "${DAL_CPP_MSVC_RUNTIME_LIBRARY}")
        message(FATAL_ERROR
            "installed consumer runtime '${consumer_runtime}' does not match DAL runtime '${DAL_CPP_MSVC_RUNTIME_LIBRARY}'")
    endif()
endif()

enable_testing()
add_test(NAME dal_installed_consumer COMMAND dal_installed_consumer)
