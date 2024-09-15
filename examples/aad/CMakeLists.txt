file(GLOB_RECURSE AAD_FILES "*.hpp" "*.cpp")
add_executable(aad ${AAD_FILES})

target_link_libraries(aad dal_library)

if(MSVC)
else()
    target_link_libraries(aad pthread)
    target_link_libraries(aad adept)
endif()

install(TARGETS aad
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )