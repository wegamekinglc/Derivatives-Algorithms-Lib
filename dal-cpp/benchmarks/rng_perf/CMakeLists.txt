file(GLOB_RECURSE RNG_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(rng_perf ${RNG_PERF_FILES})

target_link_libraries(rng_perf dal_library)


if(MSVC)
else()
    target_link_libraries(rng_perf pthread)
endif()

install(TARGETS rng_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME rng_perf COMMAND rng_perf)
set_tests_properties(rng_perf PROPERTIES LABELS "benchmark")
