file(GLOB_RECURSE SOBOL_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(sobol ${SOBOL_FILES})

target_link_libraries(sobol dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(sobol XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(sobol CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(sobol adept)
endif ()

if(MSVC)
else()
    target_link_libraries(sobol pthread)
endif()

install(TARGETS sobol
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
