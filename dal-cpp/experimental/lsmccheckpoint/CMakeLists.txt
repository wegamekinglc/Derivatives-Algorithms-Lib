cmake_minimum_required(VERSION 3.21)
project(lsmc_checkpoint_study LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(DAL_BUILD_PUBLIC OFF CACHE BOOL "" FORCE)
set(DAL_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
set(DAL_BUILD_EXCEL OFF CACHE BOOL "" FORCE)
set(DAL_BUILD_EXCEL_PORTABLE_TESTS OFF CACHE BOOL "" FORCE)
set(DAL_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAL_CPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(DAL_CPP_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../.." dal)

add_executable(lsmc_checkpoint_study checkpointstudy.cpp)
target_link_libraries(lsmc_checkpoint_study PRIVATE DAL::cpp)
dal_apply_platform_options(lsmc_checkpoint_study)

add_executable(lsmc_checkpoint_production production.cpp)
target_link_libraries(lsmc_checkpoint_production PRIVATE DAL::cpp)
dal_apply_platform_options(lsmc_checkpoint_production)

enable_testing()
foreach(block IN ITEMS 1 4 17 23)
    add_test(NAME checkpoint_block_${block} COMMAND lsmc_checkpoint_study
        --schedule verify --events 17 --block ${block} --training 257 --validation 73 --pricing 129)
endforeach()
foreach(block IN ITEMS 1 4 8 65 79)
    add_test(NAME production_checkpoint_block_${block} COMMAND lsmc_checkpoint_production
        --schedule verify --events 65 --block ${block} --training 257 --validation 73 --pricing 129 --bridge true)
endforeach()
add_test(NAME checkpoint_vector_bridge COMMAND lsmc_checkpoint_study
    --schedule verify --events 13 --block 4 --training 257 --validation 73 --pricing 129
    --growing-vector true --bridge true)
