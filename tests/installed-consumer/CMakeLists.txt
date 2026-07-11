cmake_minimum_required(VERSION 3.21)

project(dal-installed-consumer LANGUAGES CXX)

find_package(dal-cpp 1.0 CONFIG REQUIRED)
find_package(dal-public 1.0 CONFIG REQUIRED)

add_executable(dal_installed_consumer main.cpp)
target_link_libraries(dal_installed_consumer PRIVATE DAL::cpp DAL::public)

enable_testing()
add_test(NAME dal_installed_consumer COMMAND dal_installed_consumer)
