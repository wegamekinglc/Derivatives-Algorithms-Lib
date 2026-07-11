if(NOT DEFINED GIT_EXECUTABLE OR NOT DEFINED DAL_REPOSITORY_ROOT)
    message(FATAL_ERROR "GIT_EXECUTABLE and DAL_REPOSITORY_ROOT are required")
endif()

set(DAL_GENERATED_PATHS
    dal-cpp/dal/auto
    dal-excel/auto)

execute_process(
    COMMAND "${GIT_EXECUTABLE}" diff --exit-code -- ${DAL_GENERATED_PATHS}
    WORKING_DIRECTORY "${DAL_REPOSITORY_ROOT}"
    RESULT_VARIABLE DAL_GENERATED_DIFF_RESULT)
if(NOT DAL_GENERATED_DIFF_RESULT EQUAL 0)
    message(FATAL_ERROR "Machinist regeneration changed tracked generated files")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" ls-files --others --exclude-standard -- ${DAL_GENERATED_PATHS}
    WORKING_DIRECTORY "${DAL_REPOSITORY_ROOT}"
    RESULT_VARIABLE DAL_GENERATED_UNTRACKED_RESULT
    OUTPUT_VARIABLE DAL_GENERATED_UNTRACKED_FILES
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT DAL_GENERATED_UNTRACKED_RESULT EQUAL 0)
    message(FATAL_ERROR "Unable to inspect untracked generated files")
endif()
if(NOT DAL_GENERATED_UNTRACKED_FILES STREQUAL "")
    message(FATAL_ERROR
        "Machinist regeneration produced untracked generated files:\n${DAL_GENERATED_UNTRACKED_FILES}")
endif()
