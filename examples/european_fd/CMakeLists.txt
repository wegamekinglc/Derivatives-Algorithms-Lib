file(GLOB_RECURSE EUROPEAN_FD_FILES "*.hpp" "*.cpp")
add_executable(european_fd ${EUROPEAN_FD_FILES})

target_link_libraries(european_fd dal_library)

if(MSVC)
else()
    target_link_libraries(european_fd pthread)
endif()

install(TARGETS european_fd
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )