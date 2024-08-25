file(GLOB_RECURSE SNOWBALL_FILES "*.hpp" "*.cpp")
add_executable(snowball ${SNOWBALL_FILES})

target_link_libraries(snowball dal_library)

if(MSVC)
else()
    target_link_libraries(snowball pthread)
endif()

install(TARGETS snowball
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )