cmake_minimum_required(VERSION 3.9)
project(derivatives_algorithms)

set(CMAKE_CXX_STANDARD 11)

if(MSVC)
    INCLUDE_DIRECTORIES($ENV{GTEST_ROOT}/include)
    link_directories($ENV{GTEST_ROOT}/lib/Release/MD)
endif()

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

add_subdirectory(dal)
add_subdirectory(tests)
add_subdirectory(examples)