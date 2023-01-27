file(GLOB_RECURSE SCRIPT_COMPILED_FILES "*.hpp" "*.cpp")
add_executable(script_compiled ${SCRIPT_COMPILED_FILES})

target_link_libraries(script_compiled dal_library)

if(MSVC)
else()
    target_link_libraries(script_compiled pthread)
endif()

install(TARGETS script_compiled
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )