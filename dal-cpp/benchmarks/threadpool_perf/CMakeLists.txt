file(GLOB_RECURSE THREADPOOL_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(threadpool_perf ${THREADPOOL_PERF_FILES})

target_link_libraries(threadpool_perf dal_library)

if(MSVC)
else()
    target_link_libraries(threadpool_perf pthread)
endif()

install(TARGETS threadpool_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME threadpool_perf COMMAND threadpool_perf)
set_tests_properties(threadpool_perf PROPERTIES LABELS "benchmark")

