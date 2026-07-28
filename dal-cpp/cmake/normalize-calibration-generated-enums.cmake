if(NOT DEFINED DAL_REPOSITORY_ROOT)
    message(FATAL_ERROR "DAL_REPOSITORY_ROOT is required")
endif()

# Machinist's legacy Enumeration template emits trailing whitespace on an
# otherwise-empty branch and more than one final newline. Keep the generated
# calibration enums reproducible without rewriting unrelated legacy output.
set(DAL_CALIBRATION_GENERATED_ENUMS
    MG_AnalyticIneligibilityReason_enum
    MG_CurveFreeParameterComponent_enum
    MG_CurveKnotCandidateDisposition_enum
    MG_CurveKnotOriginKind_enum
    MG_RateInstrumentType_enum)

set(DAL_CALIBRATION_GENERATED_PATHS)
foreach(DAL_ENUM_NAME IN LISTS DAL_CALIBRATION_GENERATED_ENUMS)
    list(APPEND DAL_CALIBRATION_GENERATED_PATHS
        "${DAL_REPOSITORY_ROOT}/dal-cpp/dal/auto/${DAL_ENUM_NAME}.hpp"
        "${DAL_REPOSITORY_ROOT}/dal-cpp/dal/auto/${DAL_ENUM_NAME}.inc")
endforeach()
list(APPEND DAL_CALIBRATION_GENERATED_PATHS
    "${DAL_REPOSITORY_ROOT}/dal-cpp/dal/auto/MG_DiscountPWC_object.hpp"
    "${DAL_REPOSITORY_ROOT}/dal-cpp/dal/auto/MG_DiscountPWC_v1_Read.inc"
    "${DAL_REPOSITORY_ROOT}/dal-cpp/dal/auto/MG_DiscountPWC_v1_Write.inc")

foreach(DAL_GENERATED_PATH IN LISTS DAL_CALIBRATION_GENERATED_PATHS)
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
