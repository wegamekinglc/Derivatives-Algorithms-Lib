if(MSVC)
    add_subdirectory(excel)
endif()

file(GLOB_RECURSE PUBLIC_FILES "src/*.hpp" "src/*.cpp")
add_definitions(-DIS_BASE)
add_library(dal_public ${PUBLIC_FILES})
target_link_libraries(dal_public dal_library)

install(TARGETS dal_public
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )