cmake_minimum_required(VERSION 3.21)

get_filename_component(DAL_NORMALIZER_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(DAL_NORMALIZER_SCRIPT
    "${DAL_NORMALIZER_REPOSITORY_ROOT}/dal-cpp/cmake/normalize-calibration-generated-enums.cmake")
if(DEFINED DAL_NORMALIZER_UNDER_TEST)
    set(DAL_NORMALIZER_SCRIPT "${DAL_NORMALIZER_UNDER_TEST}")
endif()

set(DAL_NORMALIZER_EXPECTED_PATHS
    dal-cpp/dal/auto/MG_AnalyticIneligibilityReason_enum.hpp
    dal-cpp/dal/auto/MG_AnalyticIneligibilityReason_enum.inc
    dal-cpp/dal/auto/MG_CurveFreeParameterComponent_enum.hpp
    dal-cpp/dal/auto/MG_CurveFreeParameterComponent_enum.inc
    dal-cpp/dal/auto/MG_CurveKnotCandidateDisposition_enum.hpp
    dal-cpp/dal/auto/MG_CurveKnotCandidateDisposition_enum.inc
    dal-cpp/dal/auto/MG_CurveKnotOriginKind_enum.hpp
    dal-cpp/dal/auto/MG_CurveKnotOriginKind_enum.inc
    dal-cpp/dal/auto/MG_RateInstrumentType_enum.hpp
    dal-cpp/dal/auto/MG_RateInstrumentType_enum.inc
    dal-cpp/dal/auto/MG_DiscountPWC_object.hpp
    dal-cpp/dal/auto/MG_DiscountPWC_v1_Read.inc
    dal-cpp/dal/auto/MG_DiscountPWC_v1_Write.inc)

file(READ "${DAL_NORMALIZER_SCRIPT}" DAL_NORMALIZER_SOURCE)
foreach(DAL_EXPECTED_PATH IN LISTS DAL_NORMALIZER_EXPECTED_PATHS)
    string(FIND "${DAL_NORMALIZER_SOURCE}" "\"${DAL_EXPECTED_PATH}\"" DAL_MANIFEST_MEMBER_POSITION)
    if(DAL_MANIFEST_MEMBER_POSITION EQUAL -1)
        message(FATAL_ERROR "Normalizer fixed manifest is missing: ${DAL_EXPECTED_PATH}")
    endif()
endforeach()
if(DAL_NORMALIZER_SOURCE MATCHES "file\\([ \t\r\n]*GLOB")
    message(FATAL_ERROR "Normalizer fixed manifest must not use broad discovery")
endif()
if(DAL_NORMALIZER_CONTRACT_ONLY)
    return()
endif()

if(DEFINED ENV{TMPDIR} AND NOT "$ENV{TMPDIR}" STREQUAL "")
    set(DAL_NORMALIZER_TEMP_PARENT "$ENV{TMPDIR}")
elseif(DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
    set(DAL_NORMALIZER_TEMP_PARENT "$ENV{TEMP}")
else()
    set(DAL_NORMALIZER_TEMP_PARENT "${CMAKE_CURRENT_BINARY_DIR}")
endif()
set(DAL_NORMALIZER_TEMP_ROOT "${DAL_NORMALIZER_TEMP_PARENT}/dal-normalizer-selftest")
file(REMOVE_RECURSE "${DAL_NORMALIZER_TEMP_ROOT}")
file(MAKE_DIRECTORY "${DAL_NORMALIZER_TEMP_ROOT}")

function(DAL_WRITE_NORMALIZER_FIXTURE DAL_FIXTURE_ROOT)
    foreach(DAL_EXPECTED_PATH IN LISTS DAL_NORMALIZER_EXPECTED_PATHS)
        get_filename_component(DAL_EXPECTED_DIRECTORY "${DAL_FIXTURE_ROOT}/${DAL_EXPECTED_PATH}" DIRECTORY)
        file(MAKE_DIRECTORY "${DAL_EXPECTED_DIRECTORY}")
        file(WRITE "${DAL_FIXTURE_ROOT}/${DAL_EXPECTED_PATH}" "alpha   \nblank\t \n\n\n")
    endforeach()
    set(DAL_LEGACY_PATH "${DAL_FIXTURE_ROOT}/dal-cpp/dal/auto/MG_Legacy_enum.hpp")
    file(WRITE "${DAL_LEGACY_PATH}" "legacy \t\n\n")
endfunction()

set(DAL_POSITIVE_ROOT "${DAL_NORMALIZER_TEMP_ROOT}/positive")
DAL_WRITE_NORMALIZER_FIXTURE("${DAL_POSITIVE_ROOT}")
file(SHA256 "${DAL_POSITIVE_ROOT}/dal-cpp/dal/auto/MG_Legacy_enum.hpp" DAL_LEGACY_BEFORE)
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DDAL_REPOSITORY_ROOT=${DAL_POSITIVE_ROOT}" -P "${DAL_NORMALIZER_SCRIPT}"
    RESULT_VARIABLE DAL_POSITIVE_RESULT
    OUTPUT_VARIABLE DAL_POSITIVE_STDOUT
    ERROR_VARIABLE DAL_POSITIVE_STDERR)
if(NOT DAL_POSITIVE_RESULT EQUAL 0)
    message(FATAL_ERROR "Production normalizer failed the complete fixed manifest: ${DAL_POSITIVE_STDOUT}${DAL_POSITIVE_STDERR}")
endif()
foreach(DAL_EXPECTED_PATH IN LISTS DAL_NORMALIZER_EXPECTED_PATHS)
    file(READ "${DAL_POSITIVE_ROOT}/${DAL_EXPECTED_PATH}" DAL_NORMALIZED_CONTENT)
    if(NOT DAL_NORMALIZED_CONTENT STREQUAL "alpha\nblank\n")
        message(FATAL_ERROR "Fixed manifest member was not normalized: ${DAL_EXPECTED_PATH}")
    endif()
endforeach()
file(SHA256 "${DAL_POSITIVE_ROOT}/dal-cpp/dal/auto/MG_Legacy_enum.hpp" DAL_LEGACY_AFTER)
if(NOT DAL_LEGACY_BEFORE STREQUAL DAL_LEGACY_AFTER)
    message(FATAL_ERROR "Unlisted legacy enum was modified")
endif()

set(DAL_MISSING_MEMBER "dal-cpp/dal/auto/MG_CurveKnotOriginKind_enum.inc")
set(DAL_MISSING_ROOT "${DAL_NORMALIZER_TEMP_ROOT}/missing")
DAL_WRITE_NORMALIZER_FIXTURE("${DAL_MISSING_ROOT}")
file(REMOVE "${DAL_MISSING_ROOT}/${DAL_MISSING_MEMBER}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" "-DDAL_REPOSITORY_ROOT=${DAL_MISSING_ROOT}" -P "${DAL_NORMALIZER_SCRIPT}"
    RESULT_VARIABLE DAL_MISSING_RESULT
    OUTPUT_VARIABLE DAL_MISSING_STDOUT
    ERROR_VARIABLE DAL_MISSING_STDERR)
if(DAL_MISSING_RESULT EQUAL 0)
    message(FATAL_ERROR "Missing fixed manifest member was accepted: ${DAL_MISSING_MEMBER}")
endif()
string(FIND "${DAL_MISSING_STDOUT}${DAL_MISSING_STDERR}" "MG_CurveKnotOriginKind_enum.inc" DAL_MISSING_NAME_POSITION)
if(DAL_MISSING_NAME_POSITION EQUAL -1)
    message(FATAL_ERROR "Missing-member failure did not name: ${DAL_MISSING_MEMBER}")
endif()

set(DAL_MANIFEST_LOSS_MEMBER "dal-cpp/dal/auto/MG_RateInstrumentType_enum.inc")
set(DAL_MUTATED_SCRIPT "${DAL_NORMALIZER_TEMP_ROOT}/normalizer-with-manifest-loss.cmake")
string(REPLACE "    \"${DAL_MANIFEST_LOSS_MEMBER}\"\n" "" DAL_MUTATED_SOURCE "${DAL_NORMALIZER_SOURCE}")
if(DAL_MUTATED_SOURCE STREQUAL DAL_NORMALIZER_SOURCE)
    message(FATAL_ERROR "Manifest-loss mutation anchor was not unique")
endif()
file(WRITE "${DAL_MUTATED_SCRIPT}" "${DAL_MUTATED_SOURCE}")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
            "-DDAL_NORMALIZER_UNDER_TEST=${DAL_MUTATED_SCRIPT}"
            -DDAL_NORMALIZER_CONTRACT_ONLY=ON
            -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE DAL_MANIFEST_LOSS_RESULT
    OUTPUT_VARIABLE DAL_MANIFEST_LOSS_STDOUT
    ERROR_VARIABLE DAL_MANIFEST_LOSS_STDERR)
if(DAL_MANIFEST_LOSS_RESULT EQUAL 0)
    message(FATAL_ERROR "Fixed manifest loss was accepted: ${DAL_MANIFEST_LOSS_MEMBER}")
endif()
string(FIND "${DAL_MANIFEST_LOSS_STDOUT}${DAL_MANIFEST_LOSS_STDERR}" "MG_RateInstrumentType_enum.inc" DAL_MANIFEST_LOSS_NAME_POSITION)
if(DAL_MANIFEST_LOSS_NAME_POSITION EQUAL -1)
    message(FATAL_ERROR "Manifest-loss failure did not name: ${DAL_MANIFEST_LOSS_MEMBER}")
endif()

file(REMOVE_RECURSE "${DAL_NORMALIZER_TEMP_ROOT}")
message(STATUS "Calibration generated normalizer fixed-manifest self-test passed")
