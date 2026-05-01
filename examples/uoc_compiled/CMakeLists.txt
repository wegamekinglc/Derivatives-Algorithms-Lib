file(GLOB_RECURSE UOC_COMPILED_FILES "*.hpp" "*.cpp")
add_executable(uoc_compiled ${UOC_COMPILED_FILES})

target_link_libraries(uoc_compiled dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(uoc_compiled XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(uoc_compiled CoDiPack)
endif ()

if(MSVC)
else()
    target_link_libraries(uoc_compiled pthread)
endif()

install(TARGETS uoc_compiled
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
