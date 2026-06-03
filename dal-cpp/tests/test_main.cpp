//
// Created by wegamekinglc on 17-12-19.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    Dal::RegisterAll_::Init();
    return RUN_ALL_TESTS();
}