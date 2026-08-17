file(GLOB_RECURSE SCRIPT_TREE_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(script_tree ${SCRIPT_TREE_FILES})

target_link_libraries(script_tree dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(script_tree XAD::xad)
elseif(DAL_USE_CODIPACK_AAD)
    target_link_libraries(script_tree CoDiPack)
elseif(DAL_USE_ADEPT_AAD)
    target_link_libraries(script_tree adept)
endif ()

if(MSVC)
else()
    target_link_libraries(script_tree pthread)
endif()

install(TARGETS script_tree
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
