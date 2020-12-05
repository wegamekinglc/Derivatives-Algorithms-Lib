file(GLOB_RECURSE DATE_UTILITIES_FILES "*.hpp" "*.cpp")
add_executable(date_utilities ${DATE_UTILITIES_FILES})

target_link_libraries(date_utilities ${DAL_LINK_LIBRARY})

if(MSVC)
else()
    target_link_libraries(date_utilities pthread)
endif()

install(TARGETS date_utilities
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )