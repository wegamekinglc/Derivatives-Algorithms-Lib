cmake_minimum_required(VERSION 3.21)
project(dal-s4-input-probe LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(DAL_INPUT_PROBE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(DAL_INPUT_PROBE_BUILD "${DAL_INPUT_PROBE_ROOT}/build/s4" CACHE PATH "Existing S4 binding build")
file(GLOB DAL_INPUT_PROBE_GENERATED CONFIGURE_DEPENDS
    "${DAL_INPUT_PROBE_ROOT}/dal-excel/auto/MG_EarlyExercisePdeRisk*_public.inc")
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/probe.cpp" "${CMAKE_CURRENT_BINARY_DIR}/source-identity.json"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
        "${DAL_INPUT_PROBE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/prepare.py"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/tests/earlyexerciseinputprobe.in"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/src/_excel.cpp"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/src/_excel.hpp"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/src/__scriptinput.hpp"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/src/__dupireinput.hpp"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/src/__earlyexercisepdeinput.hpp"
        "${DAL_INPUT_PROBE_ROOT}/dal-excel/src/__earlyexercisepderiskinput.hpp"
        ${DAL_INPUT_PROBE_GENERATED}
    VERBATIM)

find_library(DAL_INPUT_PROBE_CORE NAMES dal_cpp PATHS "${DAL_INPUT_PROBE_BUILD}/dal-cpp" NO_DEFAULT_PATH REQUIRED)
find_library(DAL_INPUT_PROBE_GTEST NAMES gtest PATHS "${DAL_INPUT_PROBE_BUILD}/lib" NO_DEFAULT_PATH REQUIRED)
find_library(DAL_INPUT_PROBE_GTEST_MAIN NAMES gtest_main PATHS "${DAL_INPUT_PROBE_BUILD}/lib" NO_DEFAULT_PATH REQUIRED)
add_executable(dal_s4_input_probe
    "${CMAKE_CURRENT_BINARY_DIR}/probe.cpp"
    "${DAL_INPUT_PROBE_BUILD}/dal-excel/CMakeFiles/dal_excel_portable_tests.dir/src/__earlyexercisepderisk.cpp.o")
target_compile_features(dal_s4_input_probe PRIVATE cxx_std_17)
target_compile_definitions(dal_s4_input_probe PRIVATE GTEST_LINKED_AS_SHARED_LIBRARY=1)
target_include_directories(dal_s4_input_probe PRIVATE
    "${DAL_INPUT_PROBE_ROOT}"
    "${DAL_INPUT_PROBE_ROOT}/dal-cpp"
    "${DAL_INPUT_PROBE_ROOT}/dal-cpp/externals/googletest/googletest/include")
target_link_libraries(dal_s4_input_probe PRIVATE
    "${DAL_INPUT_PROBE_CORE}" "${DAL_INPUT_PROBE_GTEST_MAIN}" "${DAL_INPUT_PROBE_GTEST}")
enable_testing()
include(GoogleTest)
gtest_discover_tests(dal_s4_input_probe)
