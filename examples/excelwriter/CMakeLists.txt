file(GLOB_RECURSE EXCELWRITER_FILES "*.hpp" "*.cpp")
add_executable(excelwriter ${EXCELWRITER_FILES})

target_link_libraries(excelwriter dal_library)

if(MSVC)
else()
    target_link_libraries(excelwriter pthread)
endif()

install(TARGETS excelwriter
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )