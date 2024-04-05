file(GLOB_RECURSE TEST_FILES "*.hpp" "*.cpp")

add_executable(test_suite ${TEST_FILES})

find_package(GTest CONFIG REQUIRED)
target_link_libraries(test_suite dal_library)
target_link_libraries(test_suite GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
if(DEFINED USE_XAD)
        target_link_libraries(test_suite xad)
endif()

install(TARGETS test_suite
        RUNTIME DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        )