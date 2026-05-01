file(GLOB_RECURSE VANILLA_FILES "*.hpp" "*.cpp")
add_executable(vanilla ${VANILLA_FILES})

target_link_libraries(vanilla dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(vanilla XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(vanilla CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(vanilla adept)
endif ()


if(MSVC)
else()
    target_link_libraries(vanilla pthread)
endif()

install(TARGETS vanilla
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
