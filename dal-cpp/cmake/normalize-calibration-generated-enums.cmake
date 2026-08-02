if(NOT DEFINED DAL_REPOSITORY_ROOT)
    message(FATAL_ERROR "DAL_REPOSITORY_ROOT is required")
endif()

# Machinist's legacy Enumeration template emits trailing whitespace on an
# otherwise-empty branch and more than one final newline. Keep this manifest
# explicit so unrelated legacy generated output is never discovered broadly.
set(DAL_CALIBRATION_GENERATED_PATHS
    "dal-cpp/dal/auto/MG_AnalyticIneligibilityReason_enum.hpp"
    "dal-cpp/dal/auto/MG_AnalyticIneligibilityReason_enum.inc"
    "dal-cpp/dal/auto/MG_CurveFreeParameterComponent_enum.hpp"
    "dal-cpp/dal/auto/MG_CurveFreeParameterComponent_enum.inc"
    "dal-cpp/dal/auto/MG_CurveKnotCandidateDisposition_enum.hpp"
    "dal-cpp/dal/auto/MG_CurveKnotCandidateDisposition_enum.inc"
    "dal-cpp/dal/auto/MG_CurveKnotOriginKind_enum.hpp"
    "dal-cpp/dal/auto/MG_CurveKnotOriginKind_enum.inc"
    "dal-cpp/dal/auto/MG_RateInstrumentType_enum.hpp"
    "dal-cpp/dal/auto/MG_RateInstrumentType_enum.inc"
    "dal-cpp/dal/auto/MG_DiscountPWC_object.hpp"
    "dal-cpp/dal/auto/MG_DiscountPWC_v1_Read.inc"
    "dal-cpp/dal/auto/MG_DiscountPWC_v1_Write.inc")

foreach(DAL_GENERATED_RELATIVE_PATH IN LISTS DAL_CALIBRATION_GENERATED_PATHS)
    set(DAL_GENERATED_PATH "${DAL_REPOSITORY_ROOT}/${DAL_GENERATED_RELATIVE_PATH}")
    if(NOT EXISTS "${DAL_GENERATED_PATH}")
        message(FATAL_ERROR "Expected generated source is missing: ${DAL_GENERATED_PATH}")
    endif()
    file(READ "${DAL_GENERATED_PATH}" DAL_GENERATED_CONTENT)
    string(REGEX REPLACE "[ \t]+\r?\n" "\n"
        DAL_GENERATED_NORMALIZED "${DAL_GENERATED_CONTENT}")
    string(REGEX REPLACE "[ \t]+$" ""
        DAL_GENERATED_NORMALIZED "${DAL_GENERATED_NORMALIZED}")
    string(REGEX REPLACE "\n+$" "\n"
        DAL_GENERATED_NORMALIZED "${DAL_GENERATED_NORMALIZED}")
    if(NOT DAL_GENERATED_NORMALIZED STREQUAL DAL_GENERATED_CONTENT)
        file(WRITE "${DAL_GENERATED_PATH}" "${DAL_GENERATED_NORMALIZED}")
    endif()
endforeach()
