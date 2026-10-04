file(GLOB_RECURSE SCRIPT_MC_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(script_mc_perf ${SCRIPT_MC_PERF_FILES})

target_link_libraries(script_mc_perf dal_library)


if(MSVC)
else()
    target_link_libraries(script_mc_perf pthread)
endif()

install(TARGETS script_mc_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME script_mc_perf COMMAND script_mc_perf)
set_tests_properties(script_mc_perf PROPERTIES LABELS "benchmark")
