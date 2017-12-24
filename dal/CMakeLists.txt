file(GLOB_RECURSE DAL_FILES "*.hpp" "*.cpp")


if(MSVC)
    add_library(dal STATIC ${DAL_FILES})
else()
    add_library(dal SHARED ${DAL_FILES})
endif()


install(DIRECTORY . DESTINATION include/dal
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )