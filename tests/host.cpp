//
// Created by wegamekinglc on 17-12-19.
//

#include <gtest/gtest.h>
#include <dal/host.hpp>

using namespace host;

TEST(HostTest, LocalTime) {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    localTime(&year, &month, &day, &hour, &minute, &second);

    time_t t = time(nullptr);
    struct  tm * now = localtime(&t);

    ASSERT_EQ(year, now->tm_year + 1900);
    ASSERT_EQ(month, now->tm_mon + 1);
    ASSERT_EQ(day, now->tm_mday);
    ASSERT_EQ(hour, now->tm_hour);
    ASSERT_EQ(minute, now->tm_min);
    ASSERT_EQ(second, now->tm_sec);
}