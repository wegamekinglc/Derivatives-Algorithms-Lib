add_executable(correlated_bs_perf correlated_bs_perf.cpp)
target_link_libraries(correlated_bs_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(correlated_bs_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(correlated_bs_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(correlated_bs_perf adept)
endif()

if(NOT MSVC)
    target_link_libraries(correlated_bs_perf pthread)
endif()

install(TARGETS correlated_bs_perf RUNTIME DESTINATION bin)
add_test(NAME correlated_bs_perf COMMAND correlated_bs_perf)
set_tests_properties(correlated_bs_perf PROPERTIES LABELS "benchmark")
