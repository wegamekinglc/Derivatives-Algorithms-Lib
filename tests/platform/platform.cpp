//
// Created by Cheng Li on 2017/12/20.
//

#include <dal/platform/platform.hpp>
#include <gtest/gtest.h>

TEST(PlatformTest, IsZeroTest) { ASSERT_TRUE(dal::IsZero(0.)); }

TEST(PlatformTest, IsPositiveTest) {
    ASSERT_FALSE(dal::IsPositive(-1.));
    ASSERT_TRUE(dal::IsPositive(1.));
}

TEST(PlatformTest, IsNegativeTest) {
    ASSERT_FALSE(dal::IsNegative(1.));
    ASSERT_TRUE(dal::IsNegative(-1.));
}

TEST(PlatformTest, SquareTest) {
    ASSERT_DOUBLE_EQ(dal::Square(2.0), 4.0);
    ASSERT_DOUBLE_EQ(dal::Square(dal::PI), dal::PI * dal::PI);
}

TEST(PlatformTest, CubeTest) { ASSERT_DOUBLE_EQ(dal::Cube(2.0), 8.0); }

TEST(PlatformTest, MaxTest) { ASSERT_DOUBLE_EQ(dal::Max(2.0, 0.), 2.0); }

TEST(PlatformTest, MinTest) { ASSERT_DOUBLE_EQ(dal::Min(2.0, dal::INFINITY), 2.); }

TEST(PlatformTest, HandleTest) {
    dal::Handle_<double> s;
    ASSERT_TRUE(s.isEmpty());
}

TEST(PlatformTest, HandleCastTest) {

    auto s = dal::handle_cast<dal::Empty_, dal::Empty_>(std::make_shared<dal::Empty_>());
    ::testing::StaticAssertTypeEq<decltype(s), dal::Handle_<dal::Empty_>>();
}