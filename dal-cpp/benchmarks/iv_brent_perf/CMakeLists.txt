file(GLOB_RECURSE IV_BRENT_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(iv_brent_perf ${IV_BRENT_PERF_FILES})

target_link_libraries(iv_brent_perf dal_library)


if(MSVC)
else()
    target_link_libraries(iv_brent_perf pthread)
endif()

install(TARGETS iv_brent_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME iv_brent_perf COMMAND iv_brent_perf)
set_tests_properties(iv_brent_perf PROPERTIES LABELS "benchmark")
