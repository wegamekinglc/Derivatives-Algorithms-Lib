file(GLOB_RECURSE SPECIALFUNCTIONS_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(specialfunctions_perf ${SPECIALFUNCTIONS_PERF_FILES})

target_link_libraries(specialfunctions_perf dal_library)


if(MSVC)
else()
    target_link_libraries(specialfunctions_perf pthread)
endif()

install(TARGETS specialfunctions_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME specialfunctions_perf COMMAND specialfunctions_perf)
set_tests_properties(specialfunctions_perf PROPERTIES LABELS "benchmark")
