file(GLOB_RECURSE SCRIPT_TREE_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(script_tree ${SCRIPT_TREE_FILES})

target_link_libraries(script_tree dal_library)


if(MSVC)
else()
    target_link_libraries(script_tree pthread)
endif()

install(TARGETS script_tree
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
