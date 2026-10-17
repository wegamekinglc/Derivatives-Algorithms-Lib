add_executable(average_option_script average_option_script.cpp)
target_link_libraries(average_option_script dal_library)


if(NOT MSVC)
    target_link_libraries(average_option_script pthread)
endif()

install(TARGETS average_option_script
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
