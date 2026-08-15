//
// Created by Cheng Li on 2017/12/19.
//

#include <dal/platform/host.hpp>
#include <gtest/gtest.h>


TEST(HostTest, TestLocalTime) {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    Dal::Host::LocalTime(&year, &month, &day, &hour, &minute, &second);

    // The clock may tick over between LocalTime() and time(nullptr), so compare
    // whole timestamps with a small tolerance instead of exact fields.
    struct tm first = {second, minute, hour, day, month - 1, year - 1900, 0, 0, -1};
    const double elapsed = difftime(time(nullptr), mktime(&first));
    ASSERT_GE(elapsed, -2.0);
    ASSERT_LE(elapsed, 2.0);
}
