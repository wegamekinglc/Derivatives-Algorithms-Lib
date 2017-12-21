//
// Created by Cheng Li on 2017/12/21.
//

#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/math/vector.hpp>


TEST(VectorTest, DefaultConstructionTest) {
    Vector_<> s;
    ASSERT_TRUE(s.empty());
}


TEST(VectorTest, SizedConstructionTest) {
    const int n = 10;
    Vector_<> s(n);
    ASSERT_EQ(s.size(), n);
}


TEST(VectorTest, SizedFilledConstructionTest) {
    const int n = 10;
    const double val = 1.;
    Vector_<> s(n, val);

    ASSERT_EQ(s.size(), n);
    for(auto i=0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}


TEST(VectorTest, ContainerConstructionTest) {
    const int n = 10;
    const double val = 1.;

    std::vector<double> src(n, val);
    Vector_<> s(src.begin(), src.end());

    ASSERT_EQ(s.size(), n);
    for(auto i=0; i != n; ++i) {
        ASSERT_DOUBLE_EQ(s[i], val);
    }
}


TEST(VectorTest, InitializedListConstructionTest) {
    Vector_<> s = {1., 2., 3., 4., 5.};

    ASSERT_EQ(s.size(), 5);
    for(auto i=0; i != 5; ++i) {
        ASSERT_DOUBLE_EQ(s[i], i+1.);
    }
}