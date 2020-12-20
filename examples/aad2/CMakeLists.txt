file(GLOB_RECURSE AAD_FILES "*.hpp" "*.cpp")
add_executable(aad2 ${AAD_FILES})

target_link_libraries(aad2 ${DAL_LINK_LIBRARY})

if(MSVC)
else()
    target_link_libraries(aad2 pthread)
endif()

install(TARGETS aad2
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )