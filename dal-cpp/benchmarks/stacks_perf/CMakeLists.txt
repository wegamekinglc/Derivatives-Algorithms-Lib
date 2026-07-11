file(GLOB_RECURSE STACKS_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(stacks_perf ${STACKS_PERF_FILES})

target_link_libraries(stacks_perf dal_library)

if(MSVC)
else()
    target_link_libraries(stacks_perf pthread)
endif()

install(TARGETS stacks_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
