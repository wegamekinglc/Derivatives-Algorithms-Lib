file(GLOB_RECURSE dag "*.hpp" "*.cpp")
add_executable(dag ${dag})

if(MSVC)
else()
    target_link_libraries(dag pthread)
endif()

install(TARGETS dag
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )