//
// Created by wegamekinglc on 17-12-19.
//

#include <gtest/gtest.h>
#include <dal/platform/initall.hpp>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Dal::InitAll();
    return RUN_ALL_TESTS();
}