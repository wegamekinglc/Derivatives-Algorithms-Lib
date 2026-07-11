file(GLOB_RECURSE IV_BRENT_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(iv_brent_perf ${IV_BRENT_PERF_FILES})

target_link_libraries(iv_brent_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(iv_brent_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(iv_brent_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(iv_brent_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(iv_brent_perf pthread)
endif()

install(TARGETS iv_brent_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
