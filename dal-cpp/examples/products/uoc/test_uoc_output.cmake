if(NOT DEFINED ENV{UOC_BINARY} OR "$ENV{UOC_BINARY}" STREQUAL "")
    message(FATAL_ERROR "UOC_BINARY must name the built UOC example")
endif()
if(NOT DEFINED UOC_PYTHON OR UOC_PYTHON STREQUAL "")
    message(FATAL_ERROR "UOC_PYTHON must name the Python interpreter")
endif()

set(ENV{DAL_NUM_THREADS} 4)
execute_process(
    COMMAND "$ENV{UOC_BINARY}" 32768
    COMMAND "${UOC_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/test_uoc_output.py"
    RESULTS_VARIABLE results
    ERROR_VARIABLE errors)

foreach(result IN LISTS results)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "UOC output test failed (${results}):\n${errors}")
    endif()
endforeach()
