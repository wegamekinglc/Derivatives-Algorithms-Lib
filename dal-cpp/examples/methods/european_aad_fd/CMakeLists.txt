add_executable(european_aad_fd european_aad_fd.cpp)
target_link_libraries(european_aad_fd dal_library)
if(NOT MSVC)
    target_link_libraries(european_aad_fd pthread)
endif()
install(TARGETS european_aad_fd
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
