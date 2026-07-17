file(GLOB_RECURSE JACOBIAN_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(jacobian_perf ${JACOBIAN_PERF_FILES})

target_link_libraries(jacobian_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(jacobian_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(jacobian_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(jacobian_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(jacobian_perf pthread)
endif()

install(TARGETS jacobian_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME jacobian_perf COMMAND jacobian_perf)
set_tests_properties(jacobian_perf PROPERTIES LABELS "benchmark")

