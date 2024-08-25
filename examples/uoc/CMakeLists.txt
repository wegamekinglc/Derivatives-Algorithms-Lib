file(GLOB_RECURSE UOC_FILES "*.hpp" "*.cpp")
add_executable(uoc ${UOC_FILES})

target_link_libraries(uoc dal_library)

if(MSVC)
else()
    target_link_libraries(uoc pthread)
endif()

install(TARGETS uoc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )