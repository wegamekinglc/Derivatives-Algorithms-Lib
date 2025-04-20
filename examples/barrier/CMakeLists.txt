file(GLOB_RECURSE BARRIER_FILES "*.hpp" "*.cpp")
add_executable(barrier ${BARRIER_FILES})

target_link_libraries(barrier dal_library)

if(MSVC)
else()
    target_link_libraries(barrier pthread)
endif()

install(TARGETS barrier
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )