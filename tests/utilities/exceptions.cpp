//
// Created by Cheng Li on 2018/1/15.
//


#include <gtest/gtest.h>
#include <dal/utilities/exceptions.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>

using Dal::Date_;
using Dal::DateTime_;

TEST(ExceptionTest, TestRequire) {
    ASSERT_THROW(REQUIRE(1 == 2, "Error"), Dal::Exception_);
}

TEST(ExceptionTest, TestNotice) {
    double x = 2.;
    int y = 1;
    NOTICE(x);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (Dal::Exception_& e) {
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
    } catch (Dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find(s);
        ASSERT_TRUE(pos != std::string::npos);
    }
}

TEST(ExceptionTest, TestNoticeWithDate) {
    const Date_ src(2017, 1, 1);
    NOTICE(src);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (Dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find("src = 2017-01-01");
        ASSERT_TRUE(pos != std::string::npos);
    }
}

TEST(ExceptionTest, TestNoticeWithDateTime) {
    const DateTime_ src(Date_(2017, 1, 1), 14, 15, 16);
    NOTICE(src);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (Dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find("src = 2017-01-01 14:15:16");
        ASSERT_TRUE(pos != std::string::npos);
    }
}

TEST(ExceptionTest, TestNoticeWithInt) {
    const int src = 1;
    NOTICE(src);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (Dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find("src = 1");
        ASSERT_TRUE(pos != std::string::npos);
    }
}

TEST(ExceptionTest, TestNoticeWithString) {
    const Dal::String_ src("hello");
    NOTICE(src);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (Dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find("src = hello");
        ASSERT_TRUE(pos != std::string::npos);
    }
}

TEST(ExceptionTest, TestNoticeWithCString) {
    const char* src = "hello";
    NOTICE(src);
    try {
        REQUIRE(1 == 2, "1 is not equal to 2!");
    } catch (Dal::Exception_& e) {
        std::string error_message = e.what();
        std::size_t pos = error_message.find("src = hello");
        ASSERT_TRUE(pos != std::string::npos);
    }
}
