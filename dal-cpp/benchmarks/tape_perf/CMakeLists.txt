file(GLOB_RECURSE TAPE_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(tape_perf ${TAPE_PERF_FILES})

target_link_libraries(tape_perf dal_library)

set(DAL_CPP_XAD_BENCHMARK_SOURCE "" CACHE PATH "Optional clean XAD v1.8.0 checkout for the tape benchmark")
if(DAL_CPP_XAD_BENCHMARK_SOURCE)
    find_package(Git REQUIRED)
    set(xad_benchmark_commit "52d5513513286d67be081761761a2363060e5cd1")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${DAL_CPP_XAD_BENCHMARK_SOURCE}" rev-parse HEAD
        OUTPUT_VARIABLE xad_benchmark_head OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE xad_benchmark_git_status)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${DAL_CPP_XAD_BENCHMARK_SOURCE}" status --porcelain
        OUTPUT_VARIABLE xad_benchmark_edits
        RESULT_VARIABLE xad_benchmark_diff_status)
    if(NOT xad_benchmark_git_status EQUAL 0 OR NOT xad_benchmark_diff_status EQUAL 0
       OR NOT xad_benchmark_head STREQUAL xad_benchmark_commit OR xad_benchmark_edits)
        message(FATAL_ERROR "The XAD benchmark requires a clean v1.8.0 checkout at ${xad_benchmark_commit}")
    endif()
    add_subdirectory("${DAL_CPP_XAD_BENCHMARK_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/xad" EXCLUDE_FROM_ALL)
    target_compile_definitions(tape_perf PRIVATE DAL_TAPE_PERF_WITH_XAD)
    target_link_libraries(tape_perf xad ${CMAKE_DL_LIBS})
endif()

if(MSVC)
else()
    target_link_libraries(tape_perf pthread)
endif()

if(NOT DAL_CPP_XAD_BENCHMARK_SOURCE)
    install(TARGETS tape_perf
            RUNTIME DESTINATION bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
            )
endif()

add_test(NAME tape_perf COMMAND tape_perf)
set_tests_properties(tape_perf PROPERTIES LABELS "benchmark")

set(dupire_verify_args --second-order --dupire --verify-only)
if(DAL_CPP_XAD_BENCHMARK_SOURCE)
    list(APPEND dupire_verify_args --with-xad)
endif()
add_test(NAME tape_perf_dupire_verify COMMAND tape_perf ${dupire_verify_args})
set_tests_properties(tape_perf_dupire_verify PROPERTIES LABELS "benchmark")
