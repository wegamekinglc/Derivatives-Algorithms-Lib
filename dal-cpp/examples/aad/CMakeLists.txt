file(GLOB_RECURSE AAD_FILES CONFIGURE_DEPENDS "*.hpp" "*.cpp")
add_executable(aad ${AAD_FILES})

target_link_libraries(aad dal_library)
target_link_libraries(aad XAD::xad)
if(DAL_USE_ADEPT_AAD)
    target_link_libraries(aad adept)
endif()

if(MSVC)
else()
    target_link_libraries(aad pthread)
endif()

install(TARGETS aad
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )
