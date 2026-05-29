# dal-cpp: Core C++ Quant Library
#
# This project contains the full internal C++ implementation of the DAL library.
# It is the foundation for all other DAL sub-projects.
#
# Two build modes:
#   Standalone:  cmake -S dal-cpp -B build
#   Monorepo:    add_subdirectory(dal-cpp) from parent CMakeLists.txt
#
# Target: dal_cpp (alias DAL::cpp)

cmake_minimum_required(VERSION 3.21.0)

# ---------------------------------------------------------------------------
# Detect build mode
# ---------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(DAL_CPP_STANDALONE TRUE)
    message(STATUS "dal-cpp: standalone mode")
else()
    set(DAL_CPP_STANDALONE FALSE)
    message(STATUS "dal-cpp: monorepo mode (added via add_subdirectory)")
endif()

# ---------------------------------------------------------------------------
# Project-level setup (standalone only)
# ---------------------------------------------------------------------------
if(DAL_CPP_STANDALONE)
    set(DAL_VERSION_MAJOR 1)
    set(DAL_VERSION_MINOR 0)
    set(DAL_VERSION_PATCH 0)
    set(DAL_VERSION ${DAL_VERSION_MAJOR}.${DAL_VERSION_MINOR}.${DAL_VERSION_PATCH})

    # Default build type
    if(NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "")
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type for single-config generators" FORCE)
    endif()

    project(dal-cpp LANGUAGES CXX
        DESCRIPTION "DAL Core C++ Quant Library"
        VERSION ${DAL_VERSION})

    message(STATUS "-- Build Mode: ${CMAKE_BUILD_TYPE}")

    # C++17
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS FALSE)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # PIC
    if(NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
        set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    endif()
    if(CMAKE_POSITION_INDEPENDENT_CODE)
        include(CheckPIESupported)
        check_pie_supported()
    endif()

    # Shared libs default ON for UNIX
    if(NOT DEFINED BUILD_SHARED_LIBS)
        set(BUILD_SHARED_LIBS ${UNIX})
    endif()
    message(STATUS "-- BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}")

    # Platform-specific flags (mirrors top-level cmake/Platform.cmake)
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
    include(Platform)

    # Installation directories
    set(DAL_INSTALL_BINDIR "bin" CACHE STRING "Installation directory for executables")
    set(DAL_INSTALL_LIBDIR "lib" CACHE STRING "Installation directory for libraries")
    set(DAL_INSTALL_INCLUDEDIR "include" CACHE STRING "Installation directory for headers")
    set(DAL_INSTALL_CMAKEDIR "lib/cmake/dal-cpp" CACHE STRING "Installation directory for CMake scripts")

    # Coverage
    if(NOT DEFINED USE_COVERAGE)
        set(USE_COVERAGE FALSE)
    endif()
endif()

# ---------------------------------------------------------------------------
# User-configurable options
# ---------------------------------------------------------------------------
option(DAL_CPP_BUILD_TESTS "Build dal-cpp tests" ON)
option(DAL_CPP_BUILD_EXAMPLES "Build dal-cpp examples" ON)

# AAD backend selection (exactly one should be ON)
if(DAL_CPP_STANDALONE)
    option(DAL_CPP_USE_XAD_AAD "Use XAD AAD backend" OFF)
    option(DAL_CPP_USE_CODIPACK_AAD "Use CoDiPack AAD backend" OFF)
    option(DAL_CPP_USE_ADEPT_AAD "Use Adept AAD backend" ON)
else()
    # In monorepo mode, inherit from parent's AAD options with CPP-prefixed fallback
    if(NOT DEFINED DAL_CPP_USE_XAD_AAD)
        if(DEFINED DAL_USE_XAD_AAD)
            set(DAL_CPP_USE_XAD_AAD ${DAL_USE_XAD_AAD})
        else()
            option(DAL_CPP_USE_XAD_AAD "Use XAD AAD backend" OFF)
        endif()
    endif()
    if(NOT DEFINED DAL_CPP_USE_CODIPACK_AAD)
        if(DEFINED DAL_USE_CODIPACK_AAD)
            set(DAL_CPP_USE_CODIPACK_AAD ${DAL_USE_CODIPACK_AAD})
        else()
            option(DAL_CPP_USE_CODIPACK_AAD "Use CoDiPack AAD backend" OFF)
        endif()
    endif()
    if(NOT DEFINED DAL_CPP_USE_ADEPT_AAD)
        if(DEFINED DAL_USE_ADEPT_AAD)
            set(DAL_CPP_USE_ADEPT_AAD ${DAL_USE_ADEPT_AAD})
        else()
            option(DAL_CPP_USE_ADEPT_AAD "Use Adept AAD backend" ON)
        endif()
    endif()
endif()

# Enforce exactly one backend
if(DAL_CPP_USE_XAD_AAD AND DAL_CPP_USE_CODIPACK_AAD)
    message(FATAL_ERROR "Only one AAD backend can be enabled (XAD and CoDiPack both ON)")
elseif(DAL_CPP_USE_XAD_AAD AND DAL_CPP_USE_ADEPT_AAD)
    message(FATAL_ERROR "Only one AAD backend can be enabled (XAD and Adept both ON)")
elseif(DAL_CPP_USE_CODIPACK_AAD AND DAL_CPP_USE_ADEPT_AAD)
    message(FATAL_ERROR "Only one AAD backend can be enabled (CoDiPack and Adept both ON)")
endif()

# ---------------------------------------------------------------------------
# AAD backend compile definitions (standalone mode)
# ---------------------------------------------------------------------------
if(DAL_CPP_STANDALONE)
    if(DAL_CPP_USE_XAD_AAD)
        message(STATUS "-- USE AAD framework: XAD")
        add_compile_definitions(DAL_USE_XAD_AAD)
    elseif(DAL_CPP_USE_CODIPACK_AAD)
        message(STATUS "-- USE AAD framework: CoDiPack")
        add_compile_definitions(DAL_USE_CODIPACK_AAD)
        add_compile_definitions(CODI_ForcedInlines)
    elseif(DAL_CPP_USE_ADEPT_AAD)
        message(STATUS "-- USE AAD framework: Adept")
        add_compile_definitions(DAL_USE_ADEPT_AAD)
        add_compile_definitions(ADEPT_FAST_EXPONENTIAL)
        add_compile_definitions(ADEPT_NO_DIMENSION_CHECKING)
        add_compile_definitions(ADEPT_NO_ALIAS_CHECKING)
        add_compile_definitions(ADEPT_INITIAL_STACK_LENGTH=10485760)
    else()
        message(STATUS "-- USE AAD framework: AADET (native)")
    endif()
endif()

# ---------------------------------------------------------------------------
# Externals (standalone mode)
# ---------------------------------------------------------------------------
if(DAL_CPP_STANDALONE)
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
endif()

# ---------------------------------------------------------------------------
# Include paths
#
# PROJECT_SOURCE_DIR resolves to dal-cpp/, and the dal/ symlink means
# #include <dal/...> resolves to dal-cpp/dal/ -> ../dal/ (the real files).
# ---------------------------------------------------------------------------
include_directories(${PROJECT_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Library target: dal_cpp (alias DAL::cpp)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE DAL_CPP_SOURCES
    "${PROJECT_SOURCE_DIR}/dal/*.h"
    "${PROJECT_SOURCE_DIR}/dal/*.hpp"
    "${PROJECT_SOURCE_DIR}/dal/*.cpp"
    "${PROJECT_SOURCE_DIR}/dal/*.inc")
list(FILTER DAL_CPP_SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/auto/.*")
list(FILTER DAL_CPP_SOURCES EXCLUDE REGEX "${PROJECT_SOURCE_DIR}/dal/storage/_repository.*")

add_library(dal_cpp ${DAL_CPP_SOURCES})
add_library(DAL::cpp ALIAS dal_cpp)

# MSVC export define (mirrors dal/CMakeLists.txt)
if(MSVC)
    target_compile_definitions(dal_cpp PRIVATE IS_BASE)
endif()

# AAD backend linking
if(DAL_CPP_USE_XAD_AAD)
    target_link_libraries(dal_cpp PUBLIC XAD::xad)
elseif(DAL_CPP_USE_CODIPACK_AAD)
    target_link_libraries(dal_cpp PUBLIC CoDiPack)
elseif(DAL_CPP_USE_ADEPT_AAD)
    target_link_libraries(dal_cpp PUBLIC adept)
endif()

# Target properties
if(DAL_CPP_STANDALONE)
    set_target_properties(dal_cpp PROPERTIES
        EXPORT_NAME cpp
        OUTPUT_NAME dal_cpp
        VERSION ${DAL_VERSION})
else()
    set_target_properties(dal_cpp PROPERTIES
        EXPORT_NAME cpp
        OUTPUT_NAME dal_cpp)
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
install(DIRECTORY ${PROJECT_SOURCE_DIR}/dal/ DESTINATION include/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_cpp
        EXPORT dal-cppTargets
        ARCHIVE DESTINATION ${DAL_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${DAL_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${DAL_INSTALL_BINDIR})

# ---------------------------------------------------------------------------
# Test target: dal_cpp_tests
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_TESTS)
    enable_testing()
    include(GoogleTest)

    file(GLOB_RECURSE TEST_FILES
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp")

    add_executable(dal_cpp_tests ${TEST_FILES})
    target_link_libraries(dal_cpp_tests PRIVATE dal_cpp)
    target_link_libraries(dal_cpp_tests PRIVATE gtest gtest_main gmock gmock_main)

    # AAD backend linking for tests
    if(DAL_CPP_USE_XAD_AAD)
        target_link_libraries(dal_cpp_tests PRIVATE XAD::xad)
    elseif(DAL_CPP_USE_CODIPACK_AAD)
        target_link_libraries(dal_cpp_tests PRIVATE CoDiPack)
    elseif(DAL_CPP_USE_ADEPT_AAD)
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
# Example targets
#
# Example CMakeLists.txt files reference dal_library and backend targets.
# In standalone mode we create a compatibility alias and ensure backend
# options are propagated so the example CMakeLists work unchanged.
# ---------------------------------------------------------------------------
if(DAL_CPP_BUILD_EXAMPLES)
    # Compatibility: examples reference dal_library, not dal_cpp
    if(NOT TARGET dal_library)
        add_library(dal_library ALIAS dal_cpp)
    endif()

    # Propagate AAD options with the names examples expect
    if(DAL_CPP_USE_XAD_AAD)
        set(DAL_USE_XAD_AAD TRUE)
        set(DAL_USE_CODIPACK_AAD FALSE)
        set(DAL_USE_ADEPT_AAD FALSE)
    elseif(DAL_CPP_USE_CODIPACK_AAD)
        set(DAL_USE_XAD_AAD FALSE)
        set(DAL_USE_CODIPACK_AAD TRUE)
        set(DAL_USE_ADEPT_AAD FALSE)
    elseif(DAL_CPP_USE_ADEPT_AAD)
        set(DAL_USE_XAD_AAD FALSE)
        set(DAL_USE_CODIPACK_AAD FALSE)
        set(DAL_USE_ADEPT_AAD TRUE)
    endif()

    # Note: XAD is always added as a subdirectory (externals/xad), so
    # XAD::xad target exists even when XAD is not the selected backend.
    # This supports the aad example which always links XAD::xad.

    set(EXAMPLE_DIRS
        aad concurrency curve_calibration digital european_mc european_fd
        matrix_perf script snowball sobol underdetermined uoc uoc_compiled vanilla)

    foreach(ex_dir ${EXAMPLE_DIRS})
        if(EXISTS "${PROJECT_SOURCE_DIR}/examples/${ex_dir}/CMakeLists.txt")
            add_subdirectory("${PROJECT_SOURCE_DIR}/examples/${ex_dir}"
                             "${CMAKE_CURRENT_BINARY_DIR}/examples/${ex_dir}")
        endif()
    endforeach()
endif()
