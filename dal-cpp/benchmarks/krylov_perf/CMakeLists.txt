file(GLOB_RECURSE KRYLOV_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(krylov_perf ${KRYLOV_PERF_FILES})

target_link_libraries(krylov_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(krylov_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(krylov_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(krylov_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(krylov_perf pthread)
endif()

install(TARGETS krylov_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME krylov_perf COMMAND krylov_perf)
set_tests_properties(krylov_perf PROPERTIES LABELS "benchmark")

