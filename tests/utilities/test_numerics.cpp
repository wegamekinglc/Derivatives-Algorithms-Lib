//
// Created by wegam on 2020/11/16.
//

#include <cmath>
#include <gtest/gtest.h>
#include <dal/platform/platform.hpp>
#include <dal/utilities/numerics.hpp>
#include <dal/utilities/exceptions.hpp>

using namespace Dal;

TEST(NumericsTest, TestAsInt) {
    auto d = 10.;
    auto val = AsInt(d);
    ASSERT_EQ(val, 10);

    d = 10.6;
    val = AsInt(d);
    ASSERT_EQ(val, 10);

    d = 3147483647.;
    ASSERT_THROW(AsInt(d), Exception_);

    d = -3147483647.;
    ASSERT_THROW(AsInt(d), Exception_);
}

TEST(NumericsTest, TestAsIntPtrdiff) {
    int a[] = {1, 2, 3, 4, 5};
    auto d = &a[1] - &a[4];
    auto val = AsInt(d);
    ASSERT_EQ(val, -3);
}

TEST(NumericsTest, TestNearestInt) {
    auto d = 10.4;
    auto val = NearestInt(d);
    ASSERT_EQ(val, 10);

    d = 10.6;
    val = NearestInt(d);
    ASSERT_EQ(val, 11);

    d = -10.6;
    val = NearestInt(d);
    ASSERT_EQ(val, -11);
}

TEST(NumericsTest, TestAccumulate2F) {
    Vector_<> s = {1., 2., 3., 4., 5.};
    auto val = Accumulate(s, std::plus<>());
    ASSERT_DOUBLE_EQ(val, 15.);
}

TEST(NumericsTest, TestAccumulate) {
    Vector_<> s = {1., 2., 3., 4., 5.};
    auto val = Accumulate(s);
    ASSERT_DOUBLE_EQ(val, 15.);
}

TEST(NumericsTest, TestInnerProduct) {
    Vector_<> s1 = {1., 2., 3., 4., 5.};
    Vector_<int> s2 = {1, 2, 3, 4, 5};

    auto val = InnerProduct(s1, s2);
    ASSERT_DOUBLE_EQ(val, 55.);
}

TEST(NumericsTest, TestL1Normalized) {
    Vector_<> s = {-1., 2., -3., 4., -5.};
    auto val = Vector::L1Normalized(s);
    auto l1norm = 15.;
    for(auto i = 0; i < s.size(); ++i)
        ASSERT_NEAR(val[i], s[i] / l1norm, 1e-9);
}

TEST(NumericsTest, TestL2Normalized) {
    Vector_<> s = {-1., 2., -3., 4., -5.};
    auto val = Vector::L2Normalized(s);
    auto l1norm = std::sqrt(55.);
    for(auto i = 0; i < s.size(); ++i)
        ASSERT_NEAR(val[i], s[i] / l1norm, 1e-9);
}

TEST(NumericsTest, TestCentralized) {
    Vector_<> s = {1., 2., 3., 4., 5.};
    auto val = Vector::Centralized(s);
    auto mean = 3.0;
    for(auto i = 0; i < s.size(); ++i)
        ASSERT_NEAR(val[i], s[i] - mean, 1e-9);
}

TEST(NumericsTest, TestCovaricance) {
    Vector_<> s1 = {1., 2., 3., 4., 5.};
    Vector_<> s2 = {-1., -2., -3., -4., -5.};
    auto val = Vector::Covariance(s1, s2);
    ASSERT_NEAR(val, -2.0, 1e-9);
}

TEST(NumericsTest, TestCorrelation) {
    Vector_<> s1 = {1., 2., 3., 4., 5.};
    Vector_<> s2 = {-1., -2., -3., -4., -5.};
    auto val = Vector::Correlation(s1, s2);
    ASSERT_NEAR(val, -1.0, 1e-9);
}
