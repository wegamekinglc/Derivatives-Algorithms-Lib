file(GLOB_RECURSE PUBLIC_FILES "*.hpp" "*.cpp")
add_definitions(-DIS_BASE)
add_library(dal_public ${PUBLIC_FILES})
target_link_libraries(dal_public dal_library)

install(DIRECTORY . DESTINATION include/public/src
        FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h")

install(TARGETS dal_public
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )