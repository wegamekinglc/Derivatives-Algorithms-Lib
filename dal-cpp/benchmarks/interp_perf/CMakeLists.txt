file(GLOB_RECURSE INTERP_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(interp_perf ${INTERP_PERF_FILES})

target_link_libraries(interp_perf dal_library)


if(MSVC)
else()
    target_link_libraries(interp_perf pthread)
endif()

install(TARGETS interp_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME interp_perf COMMAND interp_perf)
set_tests_properties(interp_perf PROPERTIES LABELS "benchmark")

add_test(NAME interp_active_interval COMMAND interp_perf --active-profile interval)
add_test(NAME interp_active_flat COMMAND interp_perf --active-profile flat)
set_tests_properties(interp_active_interval interp_active_flat PROPERTIES LABELS "benchmark")
