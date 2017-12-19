cmake_minimum_required(VERSION 3.9)
project(derivatives_algorithms)

set(CMAKE_CXX_STANDARD 11)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR})

add_subdirectory(dal)
add_subdirectory(tests)
add_subdirectory(examples)