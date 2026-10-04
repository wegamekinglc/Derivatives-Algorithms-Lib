file(GLOB_RECURSE XCCY_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(xccy_perf ${XCCY_PERF_FILES})

target_link_libraries(xccy_perf dal_library)


if(NOT MSVC)
    target_link_libraries(xccy_perf pthread)
endif()

install(TARGETS xccy_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

add_test(NAME xccy_perf COMMAND xccy_perf)
set_tests_properties(xccy_perf PROPERTIES LABELS "benchmark")
