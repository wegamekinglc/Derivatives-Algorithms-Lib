cmake_minimum_required(VERSION 3.21)
project(dal_native_aad_header LANGUAGES CXX)

add_executable(native_aad_header main.cpp)
target_compile_features(native_aad_header PRIVATE cxx_std_17)
target_include_directories(native_aad_header PRIVATE "${DAL_NATIVE_AAD_ROOT}/dal-cpp")
if(DEFINED DAL_REMOVED_AAD_MACRO)
    target_compile_definitions(native_aad_header PRIVATE "${DAL_REMOVED_AAD_MACRO}")
endif()
