file(GLOB_RECURSE PUBLIC_FILES "*.hpp" "*.cpp")

add_library(dal_public SHARED ${PUBLIC_FILES})

target_link_libraries(dal_public dal_library)

install(TARGETS dal_public
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )