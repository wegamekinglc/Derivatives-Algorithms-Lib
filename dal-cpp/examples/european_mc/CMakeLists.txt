file(GLOB_RECURSE EUROPEAN_MC_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(european_mc ${EUROPEAN_MC_FILES})

target_link_libraries(european_mc dal_library)
if(DAL_USE_XAD_AAD)
    target_link_libraries(european_mc XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(european_mc CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(european_mc adept)
endif ()

if(MSVC)
else()
    target_link_libraries(european_mc pthread)
endif()

install(TARGETS european_mc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
