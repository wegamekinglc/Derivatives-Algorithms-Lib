file(GLOB_RECURSE PDE_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(pde_perf ${PDE_PERF_FILES})

target_link_libraries(pde_perf dal_library)


if(MSVC)
else()
    target_link_libraries(pde_perf pthread)
endif()

install(TARGETS pde_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME pde_perf COMMAND pde_perf)
set_tests_properties(pde_perf PROPERTIES LABELS "benchmark")
