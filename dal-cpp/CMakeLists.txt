# dal-cpp: Core C++ Quant Library
#
# Always built as a sub-project of the DAL workspace.
# Target: dal_cpp (alias DAL::cpp)

cmake_minimum_required(VERSION 3.21.0)

project(dal-cpp LANGUAGES CXX
    DESCRIPTION "DAL Core C++ Quant Library"
    VERSION 1.0.0)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(DAL_CPP_BUILD_TESTS "Build dal-cpp tests" ON)
option(DAL_CPP_BUILD_EXAMPLES "Build dal-cpp examples" ON)

option(DAL_USE_XAD_AAD "Use XAD AAD backend" OFF)
option(DAL_USE_CODIPACK_AAD "Use CoDiPack AAD backend" OFF)
option(DAL_USE_ADEPT_AAD "Use Adept AAD backend" ON)

# ---------------------------------------------------------------------------
# C++ standard, PIC, build settings
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS FALSE)

if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
if(CMAKE_POSITION_INDEPENDENT_CODE)
    include(CheckPIESupported)
    check_pie_supported()
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${UNIX})
endif()

# Platform-specific compiler flags
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
include(Platform)

# ---------------------------------------------------------------------------
# AAD backend selection
# ---------------------------------------------------------------------------
if((DAL_USE_XAD_AAD AND DAL_USE_CODIPACK_AAD) OR
   (DAL_USE_XAD_AAD AND DAL_USE_ADEPT_AAD) OR
   (DAL_USE_CODIPACK_AAD AND DAL_USE_ADEPT_AAD))
    message(FATAL_ERROR "Only one external AAD backend can be enabled")
endif()

if(DAL_USE_XAD_AAD)
    message(STATUS "USE AAD framework: XAD")
    add_compile_definitions(DAL_USE_XAD_AAD)
elseif(DAL_USE_CODIPACK_AAD)
    message(STATUS "USE AAD framework: CoDiPack")
    add_compile_definitions(DAL_USE_CODIPACK_AAD)
    add_compile_definitions(CODI_ForcedInlines)
elseif(DAL_USE_ADEPT_AAD)
    message(STATUS "USE AAD framework: Adept")
    add_compile_definitions(DAL_USE_ADEPT_AAD)
    add_compile_definitions(ADEPT_FAST_EXPONENTIAL)
    add_compile_definitions(ADEPT_NO_DIMENSION_CHECKING)
    add_compile_definitions(ADEPT_NO_ALIAS_CHECKING)
    add_compile_definitions(ADEPT_INITIAL_STACK_LENGTH=10485760)
else()
    message(STATUS "USE AAD framework: AADET (native)")
endif()

# ---------------------------------------------------------------------------
# External dependencies
# ---------------------------------------------------------------------------
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/googletest)
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/rapidjson)
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/xad)
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/CodiPack)
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/adept)
add_subdirectory(${PROJECT_SOURCE_DIR}/externals/machinist)

include_directories(${PROJECT_SOURCE_DIR}/externals/rapidjson/include)
include_directories(${PROJECT_SOURCE_DIR}/externals/xad/include)
include_directories(${PROJECT_SOURCE_DIR}/externals/CodiPack/include)
include_directories(${PROJECT_SOURCE_DIR}/externals/adept/include)

# Make <dal/...> includes work
include_directories(${PROJECT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Library target: dal_cpp (alias DAL::cpp)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE DAL_CPP_SOURCES
    "${PROJECT_SOURCE_DIR}/dal/*.h"
    "${PROJECT_SOURCE_DIR}/dal/*.hpp"
    "${PROJECT_SOURCE_DIR}/dal/*.cpp"
    "${PROJECT_SOURCE_DIR}/dal/*.inc")
list(FILTER DAL_CPP_SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/storage/_repository.*")

add_library(dal_cpp ${DAL_CPP_SOURCES})
add_library(DAL::cpp ALIAS dal_cpp)

# Make <dal/...> available to consumers (dal-public, etc.)
target_include_directories(dal_cpp PUBLIC ${PROJECT_SOURCE_DIR})

if(MSVC)
    target_compile_definitions(dal_cpp PRIVATE IS_BASE)
endif()

if(DAL_USE_XAD_AAD)
    target_link_libraries(dal_cpp PUBLIC XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(dal_cpp PUBLIC CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(dal_cpp PUBLIC adept)
endif()

set_target_properties(dal_cpp PROPERTIES
    EXPORT_NAME cpp
    OUTPUT_NAME dal_cpp
    VERSION ${PROJECT_VERSION})

install(DIRECTORY ${PROJECT_SOURCE_DIR}/dal/ DESTINATION include/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_cpp
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_TESTS)
    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")

    add_executable(dal_cpp_tests ${TEST_FILES})
    target_link_libraries(dal_cpp_tests PRIVATE dal_cpp gtest gtest_main gmock gmock_main)

    if(DAL_USE_XAD_AAD)
        target_link_libraries(dal_cpp_tests PRIVATE XAD::xad)
    elseif(DAL_USE_CODIPACK_AAD)
        target_link_libraries(dal_cpp_tests PRIVATE CoDiPack)
    elseif(DAL_USE_ADEPT_AAD)
        target_link_libraries(dal_cpp_tests PRIVATE adept)
    endif()

    gtest_discover_tests(dal_cpp_tests)

    install(TARGETS dal_cpp_tests
            RUNTIME DESTINATION bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_EXAMPLES)
    # Examples reference dal_library — provide compatibility alias
    if(NOT TARGET dal_library)
        add_library(dal_library ALIAS dal_cpp)
    endif()

    add_subdirectory(${PROJECT_SOURCE_DIR}/examples)
endif()
