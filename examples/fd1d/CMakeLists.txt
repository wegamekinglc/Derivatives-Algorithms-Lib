file(GLOB_RECURSE FD1D_FILES "*.hpp" "*.cpp")
add_executable(fd1d ${FD1D_FILES})

target_link_libraries(fd1d dal_library)

if(MSVC)
else()
    target_link_libraries(fd1d pthread)
endif()

install(TARGETS fd1d
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )