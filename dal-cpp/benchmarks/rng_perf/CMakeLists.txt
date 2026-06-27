file(GLOB_RECURSE RNG_PERF_FILES "*.hpp" "*.cpp")

add_executable(rng_perf ${RNG_PERF_FILES})

target_link_libraries(rng_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(rng_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(rng_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(rng_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(rng_perf pthread)
endif()

install(TARGETS rng_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
