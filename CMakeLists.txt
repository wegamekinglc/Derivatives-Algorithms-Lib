cmake_minimum_required(VERSION 3.0)

project(dal VERSION 1.0.0)

include(${CMAKE_CURRENT_LIST_DIR}/cmake/dal.cmake)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

message("-- Build Mode: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    INCLUDE_DIRECTORIES($ENV{GTEST_ROOT}/include)

    if ("${MSVC_RUNTIME}" STREQUAL "")
        set(MSVC_RUNTIME "dynamic")
    endif ()

    if ("${MSVC_RUNTIME}" STREQUAL "static")
        link_directories($ENV{GTEST_ROOT}/lib/${CMAKE_BUILD_TYPE}/MT)
    else()
        link_directories($ENV{GTEST_ROOT}/lib/${CMAKE_BUILD_TYPE}/MD)
    endif()
    
    set_property(CACHE MSVC_RUNTIME PROPERTY STRINGS static dynamic)
    configure_msvc_runtime()

    # Remove warnings
    add_definitions(-D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS)

else()
    set(CMAKE_CXX_STANDARD 14)
endif()

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

add_subdirectory(dal)
add_subdirectory(tests)
add_subdirectory(examples)