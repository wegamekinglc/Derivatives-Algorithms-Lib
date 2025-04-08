file(GLOB_RECURSE VANILLA_FILES "*.hpp" "*.cpp")
add_executable(vanilla ${VANILLA_FILES})

target_link_libraries(vanilla dal_library)

if(MSVC)
else()
    target_link_libraries(vanilla pthread)
endif()

install(TARGETS vanilla
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )