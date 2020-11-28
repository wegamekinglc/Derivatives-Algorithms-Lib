//
// Created by wegamekinglc on 17-12-19.
//

#include <gtest/gtest.h>
#include <dal/time/calendars/init.hpp>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Dal::Calendars_::Init();
    return RUN_ALL_TESTS();
}