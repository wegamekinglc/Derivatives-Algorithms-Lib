file(GLOB_RECURSE FIX_MC_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(fix_mc ${FIX_MC_FILES})

target_link_libraries(fix_mc dal_library)


if(MSVC)
else()
    target_link_libraries(fix_mc pthread)
endif()

install(TARGETS fix_mc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
