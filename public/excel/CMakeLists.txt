file(GLOB_RECURSE PUBLIC_FILES "../src/*.hpp" "../src/*.cpp" "*.hpp" "*.cpp")
if (MSVC)
    add_definitions(-DIS_BASE)
endif()
add_library(dal_public SHARED ${PUBLIC_FILES})
if (MSVC)
    set_target_properties(dal_public PROPERTIES SUFFIX .xll)
endif()
target_link_libraries(dal_public dal_library)

install(TARGETS dal_public
        RUNTIME DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )