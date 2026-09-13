//
// Created by Cheng Li on 2018/1/15.
//

#include <gtest/gtest.h>

#include <array>
#include <future>
#include <thread>

#include <dal/platform/platform.hpp>
#include <dal/time/date.hpp>
#include <dal/time/datetime.hpp>
#include <dal/utilities/exceptions.hpp>

using Dal::Date_;
using Dal::DateTime_;

namespace {
#if defined(_MSC_VER) || defined(__BORLANDC__)
    const std::string BASE_MESSAGE = "TestException: \n  exceptions.cpp(42): \nfailure";
#else
    const std::string BASE_MESSAGE = "\nexceptions.cpp:42: In function `TestException': \nfailure";
#endif

    [[noreturn]] void ThrowException() { throw Dal::Exception_("exceptions.cpp", 42, "TestException", "failure"); }

    std::string ThrowAndCatch() {
        try {
            ThrowException();
        } catch (const Dal::Exception_& error) {
            return error.what();
        }
    }
} // namespace

TEST(ExceptionTest, TestThrowOnJoinedThread) {
    std::string message;
    std::thread worker([&] { message = ThrowAndCatch(); });
    worker.join();
    ASSERT_EQ(message, BASE_MESSAGE);
}

TEST(ExceptionTest, TestNestedContextOrderAndCleanup) {
    std::array<std::string, 5> messages;
    std::thread worker([&] {
        {
            NOTE("outer");
            {
                const int value = 7;
                NOTICE(value);
                NOTE("inner");
                messages[0] = ThrowAndCatch();
            }
            messages[1] = ThrowAndCatch();
            try {
                NOTE("unwinding");
                ThrowException();
            } catch (const Dal::Exception_& error) {
                messages[2] = error.what();
            }
            messages[3] = ThrowAndCatch();
        }
        messages[4] = ThrowAndCatch();
    });
    worker.join();
    ASSERT_EQ(messages[0], BASE_MESSAGE + "\nouter\nvalue = 7\ninner");
    ASSERT_EQ(messages[1], BASE_MESSAGE + "\nouter");
    ASSERT_EQ(messages[2], BASE_MESSAGE + "\nouter\nunwinding");
    ASSERT_EQ(messages[3], BASE_MESSAGE + "\nouter");
    ASSERT_EQ(messages[4], BASE_MESSAGE);
}

TEST(ExceptionTest, TestContextIsolationAcrossJoinedThreads) {
    std::string parentMessage;
    std::string workerMessage;
    std::string bareMessage;
    {
        NOTE("parent");
        std::promise<void> ready;
        std::promise<void> release;
        auto readyFuture = ready.get_future();
        auto releaseFuture = release.get_future();
        std::thread worker([&] {
            NOTE("worker");
            ready.set_value();
            releaseFuture.wait();
            workerMessage = ThrowAndCatch();
        });
        readyFuture.wait();
        parentMessage = ThrowAndCatch();
        std::thread bare([&] { bareMessage = ThrowAndCatch(); });
        bare.join();
        release.set_value();
        worker.join();
    }
    ASSERT_EQ(parentMessage, BASE_MESSAGE + "\nparent");
    ASSERT_EQ(workerMessage, BASE_MESSAGE + "\nworker");
    ASSERT_EQ(bareMessage, BASE_MESSAGE);
    ASSERT_EQ(ThrowAndCatch(), BASE_MESSAGE);
}

TEST(ExceptionTest, TestEmptyPopAndStackReuse) {
    std::array<std::string, 5> messages;
    std::thread worker([&] {
        Dal::exception::PopStack();
        messages[0] = ThrowAndCatch();
        Dal::exception::PushStack(Dal::exception::XStackInfo_("manual"));
        messages[1] = ThrowAndCatch();
        Dal::exception::PopStack();
        Dal::exception::PopStack();
        messages[2] = ThrowAndCatch();
        {
            NOTE("reused");
            messages[3] = ThrowAndCatch();
        }
        messages[4] = ThrowAndCatch();
    });
    worker.join();
    ASSERT_EQ(messages[0], BASE_MESSAGE);
    ASSERT_EQ(messages[1], BASE_MESSAGE + "\nmanual");
    ASSERT_EQ(messages[2], BASE_MESSAGE);
    ASSERT_EQ(messages[3], BASE_MESSAGE + "\nreused");
    ASSERT_EQ(messages[4], BASE_MESSAGE);
}

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
