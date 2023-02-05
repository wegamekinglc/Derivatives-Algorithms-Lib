include_directories("$ENV{ADEPT_ROOT}/include")

file(GLOB_RECURSE AAD_FILES "*.hpp" "*.cpp")
add_executable(aad3 ${AAD_FILES})

target_link_libraries(aad3 dal_library)

if(MSVC)
else()
    target_link_libraries(aad3 pthread)
endif()

install(TARGETS aad3
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )