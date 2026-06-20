//
// Created by Cheng Li on 2017/12/20.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>

TEST(PlatformTest, TestIsZero) { ASSERT_TRUE(Dal::IsZero(0.)); }

TEST(PlatformTest, TestIsPositive) {
    ASSERT_FALSE(Dal::IsPositive(-1.));
    ASSERT_TRUE(Dal::IsPositive(1.));
}

TEST(PlatformTest, TestIsNegative) {
    ASSERT_FALSE(Dal::IsNegative(1.));
    ASSERT_TRUE(Dal::IsNegative(-1.));
}

TEST(PlatformTest, TestSquare) {
    ASSERT_DOUBLE_EQ(Dal::Square(2.0), 4.0);
    ASSERT_DOUBLE_EQ(Dal::Square(Dal::PI), Dal::PI * Dal::PI);
}

TEST(PlatformTest, TestCube) { ASSERT_DOUBLE_EQ(Dal::Cube(2.0), 8.0); }

TEST(PlatformTest, TestMax) { ASSERT_DOUBLE_EQ(Dal::max(2.0, 0.), 2.0); }

TEST(PlatformTest, TestMin) { ASSERT_DOUBLE_EQ(Dal::min(2.0, 20.), 2.); }

TEST(PlatformTest, TestHandle) {
    Dal::Handle_<double> s;
    ASSERT_TRUE(s.IsEmpty());
}

TEST(PlatformTest, TestHandleCast) {

    auto s = Dal::handle_cast<Dal::Empty_, Dal::Empty_>(std::make_shared<Dal::Empty_>());
    ::testing::StaticAssertTypeEq<decltype(s), Dal::Handle_<Dal::Empty_>>();
}
