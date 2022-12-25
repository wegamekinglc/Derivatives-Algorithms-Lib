file(GLOB_RECURSE UNDERDETERMINED_FILES "*.hpp" "*.cpp")
add_executable(underdetermined ${UNDERDETERMINED_FILES})

target_link_libraries(underdetermined dal_library)

if(MSVC)
else()
    target_link_libraries(underdetermined pthread)
endif()

install(TARGETS underdetermined
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )