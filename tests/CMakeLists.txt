file(GLOB_RECURSE TEST_FILES "*.hpp" "*.cpp")

add_executable(test_suite ${TEST_FILES})
if(MSVC)
    find_package(GTest CONFIG REQUIRED)
    target_link_libraries(test_suite dal_library)
    target_link_libraries(test_suite GTest::gtest GTest::gtest_main)
else()
    target_link_libraries(test_suite pthread)
    target_link_libraries(test_suite dal_library)
    target_link_libraries(test_suite gtest)
    target_link_libraries(test_suite gtest_main)
endif()



install(TARGETS test_suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )