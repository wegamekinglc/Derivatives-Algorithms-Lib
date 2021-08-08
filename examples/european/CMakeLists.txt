file(GLOB_RECURSE EUROPEAN_FILES "*.hpp" "*.cpp")
add_executable(european ${EUROPEAN_FILES})

target_link_libraries(european ${DAL_LINK_LIBRARY})

if(MSVC)
else()
    target_link_libraries(european pthread)
endif()

install(TARGETS european
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )