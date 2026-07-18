file(GLOB_RECURSE TAPE_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(tape_perf ${TAPE_PERF_FILES})

target_link_libraries(tape_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(tape_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(tape_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(tape_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(tape_perf pthread)
endif()

install(TARGETS tape_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME tape_perf COMMAND tape_perf)
set_tests_properties(tape_perf PROPERTIES LABELS "benchmark")

