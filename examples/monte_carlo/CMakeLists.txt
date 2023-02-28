file(GLOB_RECURSE MONTE_CARLO_FILES "*.hpp" "*.cpp")
add_executable(monte_carlo ${MONTE_CARLO_FILES})

target_link_libraries(monte_carlo dal_library)

if(MSVC)
else()
    target_link_libraries(monte_carlo pthread)
endif()

install(TARGETS monte_carlo
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )