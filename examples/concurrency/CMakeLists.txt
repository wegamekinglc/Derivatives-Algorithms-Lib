file(GLOB_RECURSE CONCURRENCY_FILES "*.hpp" "*.cpp")
add_executable(concurrency ${CONCURRENCY_FILES})

target_link_libraries(concurrency dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(concurrency XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(concurrency CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(concurrency adept)
endif ()

if(MSVC)
else()
    target_link_libraries(concurrency pthread)
endif()

install(TARGETS concurrency
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
