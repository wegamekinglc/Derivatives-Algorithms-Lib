file(GLOB_RECURSE RATE_RISK_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(rate_risk_perf ${RATE_RISK_PERF_FILES})

target_link_libraries(rate_risk_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(rate_risk_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(rate_risk_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(rate_risk_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(rate_risk_perf pthread)
endif()

install(TARGETS rate_risk_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME rate_risk_perf COMMAND rate_risk_perf)
set_tests_properties(rate_risk_perf PROPERTIES LABELS "benchmark")
