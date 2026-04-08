file(GLOB_RECURSE DIGITAL_FILES "*.hpp" "*.cpp")
add_executable(digital ${DIGITAL_FILES})

target_link_libraries(digital dal_library)

if(DAL_USE_XAD_AAD)
    target_link_libraries(digital XAD::xad)
endif ()

if(MSVC)
else()
    target_link_libraries(digital pthread)
endif()

install(TARGETS digital
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )