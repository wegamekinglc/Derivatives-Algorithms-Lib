//
// Created by Cheng Li on 2017/12/20.
//


#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>


TEST(PlatformTest, IsZeroTest) {
    ASSERT_TRUE(IsZero(0.));
}


TEST(PlatformTest, IsPositiveTest) {
    ASSERT_FALSE(IsPositive(-1.));
    ASSERT_TRUE(IsPositive(1.));
}


TEST(PlatformTest, IsNegativeTest) {
    ASSERT_FALSE(IsNegative(1.));
    ASSERT_TRUE(IsNegative(-1.));
}


TEST(PlatformTest, SquareTest) {
    ASSERT_DOUBLE_EQ(Square(2.0), 4.0);
    ASSERT_DOUBLE_EQ(Square(dal::PI), dal::PI * dal::PI);
}


TEST(PlatformTest, CubeTest) {
    ASSERT_DOUBLE_EQ(Cube(2.0), 8.0);
}


TEST(PlatformTest, MaxTest) {
    ASSERT_DOUBLE_EQ(Max(2.0, 0.), 2.0);
}


TEST(PlatformTest, MinTest) {
    ASSERT_DOUBLE_EQ(Min(2.0, dal::INFINITY), 2.);
}


TEST(PlatformTest, HandleTest) {
    Handle_<double> s;
    ASSERT_TRUE(s.isEmpty());
}


TEST(PlatformTest, HandleCastTest) {

    auto s = handle_cast<Empty_, Empty_>(std::make_shared<Empty_>());
    ::testing::StaticAssertTypeEq<decltype(s), Handle_<Empty_>>();
}