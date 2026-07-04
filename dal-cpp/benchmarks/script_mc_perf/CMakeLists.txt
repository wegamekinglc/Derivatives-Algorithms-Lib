file(GLOB_RECURSE SCRIPT_MC_PERF_FILES "*.hpp" "*.cpp")

add_executable(script_mc_perf ${SCRIPT_MC_PERF_FILES})

target_link_libraries(script_mc_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(script_mc_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(script_mc_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(script_mc_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(script_mc_perf pthread)
endif()

install(TARGETS script_mc_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
