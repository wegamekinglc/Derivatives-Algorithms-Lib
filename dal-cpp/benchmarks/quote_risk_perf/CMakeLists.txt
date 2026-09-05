add_executable(quote_risk_perf quote_risk_perf.cpp ../rate_risk_perf/quoteriskbenchfixtures.cpp)

target_link_libraries(quote_risk_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(quote_risk_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(quote_risk_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(quote_risk_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(quote_risk_perf pthread)
endif()

install(TARGETS quote_risk_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )

add_test(NAME quote_risk_perf COMMAND quote_risk_perf)
set_tests_properties(quote_risk_perf PROPERTIES LABELS "benchmark")
