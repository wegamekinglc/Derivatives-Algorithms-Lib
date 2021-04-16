//
// Created by Cheng Li on 17-12-19.
//

#include <dal/platform/host.hpp>
#include <gtest/gtest.h>


TEST(HostTest, LocalTimeTest) {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    Dal::Host::localTime(&year, &month, &day, &hour, &minute, &second);

    time_t t = time(nullptr);
    struct tm now = {0, 0, 0, 0, 0, 0, 0, 0, 0};
#ifdef _MSC_VER
    localtime_s(&now, &t);
#else
    localtime_r(&t, &now);
#endif

    ASSERT_EQ(year, now.tm_year + 1900);
    ASSERT_EQ(month, now.tm_mon + 1);
    ASSERT_EQ(day, now.tm_mday);
    ASSERT_EQ(hour, now.tm_hour);
    ASSERT_EQ(minute, now.tm_min);
    ASSERT_EQ(second, now.tm_sec);
}