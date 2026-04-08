file(GLOB_RECURSE TEST_FILES "*.hpp" "*.cpp")

add_executable(test_suite ${TEST_FILES})

target_link_libraries(test_suite dal_library)
target_link_libraries(test_suite gtest gtest_main gmock gmock_main)
if(DAL_USE_XAD_AAD)
        target_link_libraries(test_suite XAD::xad)
endif ()

install(TARGETS test_suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )