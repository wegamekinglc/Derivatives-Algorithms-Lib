file(GLOB_RECURSE BLACK_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(black_perf ${BLACK_PERF_FILES})

target_link_libraries(black_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(black_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(black_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(black_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(black_perf pthread)
endif()

install(TARGETS black_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME black_perf COMMAND black_perf)
set_tests_properties(black_perf PROPERTIES LABELS "benchmark")

