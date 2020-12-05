file(GLOB_RECURSE AAD_TOY_FILES "*.hpp" "*.cpp")
add_executable(aad_toy2 ${AAD_TOY_FILES})

target_link_libraries(aad_toy2 ${DAL_LINK_LIBRARY})

if(MSVC)
else()
    target_link_libraries(aad_toy2 pthread)
endif()

install(TARGETS aad_toy2
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )