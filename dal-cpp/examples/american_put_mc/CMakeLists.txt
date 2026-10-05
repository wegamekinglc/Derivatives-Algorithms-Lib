file(GLOB_RECURSE AMERICAN_PUT_MC_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(american_put_mc ${AMERICAN_PUT_MC_FILES})

target_link_libraries(american_put_mc dal_library)

if(MSVC)
else()
    target_link_libraries(american_put_mc pthread)
endif()

install(TARGETS american_put_mc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
