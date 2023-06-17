file(GLOB_RECURSE SCRIPT_FILES "*.hpp" "*.cpp")
add_executable(script ${SCRIPT_FILES})

target_link_libraries(script dal_library)

if(MSVC)
else()
    target_link_libraries(script pthread)
endif()

install(TARGETS snowball
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )