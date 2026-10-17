file(GLOB_RECURSE YCINSTRUMENT_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(ycinstrument_perf ${YCINSTRUMENT_PERF_FILES})

target_link_libraries(ycinstrument_perf dal_library)


if(MSVC)
else()
    target_link_libraries(ycinstrument_perf pthread)
endif()

install(TARGETS ycinstrument_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME ycinstrument_perf COMMAND ycinstrument_perf)
set_tests_properties(ycinstrument_perf PROPERTIES LABELS "benchmark")
