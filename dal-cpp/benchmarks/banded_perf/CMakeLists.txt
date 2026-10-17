file(GLOB_RECURSE BANDED_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(banded_perf ${BANDED_PERF_FILES})

target_link_libraries(banded_perf dal_library)


if(MSVC)
else()
    target_link_libraries(banded_perf pthread)
endif()

install(TARGETS banded_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME banded_perf COMMAND banded_perf)
set_tests_properties(banded_perf PROPERTIES LABELS "benchmark")
