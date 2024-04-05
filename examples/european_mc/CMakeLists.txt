file(GLOB_RECURSE EUROPEAN_MC_FILES "*.hpp" "*.cpp")
add_executable(european_mc ${EUROPEAN_MC_FILES})

target_link_libraries(european_mc dal_library)

if(DEFINED USE_XAD)
    target_link_libraries(european_mc xad)
endif()

if(MSVC)
else()
    target_link_libraries(european_mc pthread)
endif()

install(TARGETS european_mc
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )