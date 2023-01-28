//
// Created by Cheng Li on 2017/12/20.
//

#include <dal/platform/platform.hpp>
#include <dal/math/operators.hpp>
#include <gtest/gtest.h>

TEST(PlatformTest, IsZeroTest) { ASSERT_TRUE(Dal::IsZero(0.)); }

TEST(PlatformTest, IsPositiveTest) {
    ASSERT_FALSE(Dal::IsPositive(-1.));
    ASSERT_TRUE(Dal::IsPositive(1.));
}

TEST(PlatformTest, IsNegativeTest) {
    ASSERT_FALSE(Dal::IsNegative(1.));
    ASSERT_TRUE(Dal::IsNegative(-1.));
}

TEST(PlatformTest, SquareTest) {
    ASSERT_DOUBLE_EQ(Dal::Square(2.0), 4.0);
    ASSERT_DOUBLE_EQ(Dal::Square(Dal::PI), Dal::PI * Dal::PI);
}

TEST(PlatformTest, CubeTest) { ASSERT_DOUBLE_EQ(Dal::Cube(2.0), 8.0); }

TEST(PlatformTest, MaxTest) { ASSERT_DOUBLE_EQ(Dal::max(2.0, 0.), 2.0); }

TEST(PlatformTest, MinTest) { ASSERT_DOUBLE_EQ(Dal::min(2.0, 20.), 2.); }

TEST(PlatformTest, HandleTest) {
    Dal::Handle_<double> s;
    ASSERT_TRUE(s.IsEmpty());
}

TEST(PlatformTest, HandleCastTest) {

    auto s = Dal::handle_cast<Dal::Empty_, Dal::Empty_>(std::make_shared<Dal::Empty_>());
    ::testing::StaticAssertTypeEq<decltype(s), Dal::Handle_<Dal::Empty_>>();
}
