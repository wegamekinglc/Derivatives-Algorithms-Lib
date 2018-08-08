file(GLOB_RECURSE DATE_UTILITIES_FILES "*.hpp" "*.cpp")
add_executable(dateutilities ${DATE_UTILITIES_FILES})

target_link_libraries(dateutilities ${DAL_LINK_LIBRARY})

if(MSVC)
else()
    target_link_libraries(dateutilities pthread)
endif()

install(TARGETS dateutilities
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )