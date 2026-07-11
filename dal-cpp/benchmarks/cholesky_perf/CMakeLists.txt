file(GLOB_RECURSE CHOLESKY_PERF_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")

add_executable(cholesky_perf ${CHOLESKY_PERF_FILES})

target_link_libraries(cholesky_perf dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(cholesky_perf XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(cholesky_perf CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(cholesky_perf adept)
endif ()

if(MSVC)
else()
    target_link_libraries(cholesky_perf pthread)
endif()

install(TARGETS cholesky_perf
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
