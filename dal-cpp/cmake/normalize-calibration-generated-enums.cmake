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
    MG_CurveKnotOriginKind_enum)

foreach(DAL_ENUM_NAME IN LISTS DAL_CALIBRATION_GENERATED_ENUMS)
    foreach(DAL_ENUM_SUFFIX hpp inc)
        set(DAL_ENUM_PATH
            "${DAL_REPOSITORY_ROOT}/dal-cpp/dal/auto/${DAL_ENUM_NAME}.${DAL_ENUM_SUFFIX}")
        if(NOT EXISTS "${DAL_ENUM_PATH}")
            message(FATAL_ERROR "Expected generated enum is missing: ${DAL_ENUM_PATH}")
        endif()
        file(READ "${DAL_ENUM_PATH}" DAL_ENUM_CONTENT)
        string(REGEX REPLACE "[ \t]+\r?\n" "\n"
            DAL_ENUM_NORMALIZED "${DAL_ENUM_CONTENT}")
        string(REGEX REPLACE "\n+$" "\n"
            DAL_ENUM_NORMALIZED "${DAL_ENUM_NORMALIZED}")
        if(NOT DAL_ENUM_NORMALIZED STREQUAL DAL_ENUM_CONTENT)
            file(WRITE "${DAL_ENUM_PATH}" "${DAL_ENUM_NORMALIZED}")
        endif()
    endforeach()
endforeach()
