file(GLOB_RECURSE CONCURRENCY_FILES "*.hpp" "*.cpp")
add_executable(concurrency ${CONCURRENCY_FILES})

target_link_libraries(concurrency dal_library)

if(MSVC)
else()
    target_link_libraries(concurrency pthread)
endif()

install(TARGETS concurrency
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )