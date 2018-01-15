//
// Created by Cheng Li on 2018/1/15.
//


#include <gtest/gtest.h>
#include <dal/utilities/exceptions.hpp>

TEST(ExceptionTest, TestRequire) { ASSERT_THROW(REQUIRE(1 == 2, "Error"), dal::Exception_); }

TEST(ExceptionTest, TestNotice) {
    double x = 2.;
    NOTICE(x);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find("x = 2.0");
        ASSERT_TRUE(pos != std::string::npos);
    }
}

TEST(ExceptionTest, TestNote) {
    const char* s = "this is a break point";
    NOTE(s);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find(s );
        ASSERT_TRUE(pos != std::string::npos);
    }
}