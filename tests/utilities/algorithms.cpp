//
// Created by Cheng Li on 2017/12/29.
//

#include <dal/platform/platform.hpp>
#include <dal/math/vector.hpp>
#include <dal/utilities/algorithms.hpp>
#include <functional>
#include <gtest/gtest.h>

TEST(AlgorothmsTest, TestTransformWithUniaryOPerator) {
    Vector_<> s = {1, 2, 3};
    Vector_<> d(3);

    Transform(s, std::negate<>(), &d);

    for (int i = 0; i != d.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i], -d[i]);
    }
}

TEST(AlgorothmsTest, TestTransformWithBinaryOperator) {
    Vector_<> s = {1, 2, 3};
    Vector_<> d(s.size());

    Transform(s, s, std::plus<>(), &d);
    for (int i = 0; i != d.size(); ++i) {
        ASSERT_DOUBLE_EQ(s[i] + s[i], d[i]);
    }
}

TEST(AlgorothmsTest, TestApply) {
    Vector_<> s1 = {1, 2, 3};
    auto s2 = Apply(std::negate<>(), s1);
    for (int i = 0; i != s2.size(); ++i) {
        ASSERT_DOUBLE_EQ(s2[i], -s1[i]);
    }
}

TEST(AlgorothmsTest, TestCopyWithPreAllocatedVector) {
    Vector_<int> s1 = {1, 2, 3};
    Vector_<int> s2(s1.size());

    Copy(s1, &s2);

    for (auto i = 0; i != s1.size(); ++i) {
        ASSERT_EQ(s1[i], s2[i]);
    }
}

TEST(AlgorothmsTest, TestCopyWithReturnedValue) {
    Vector_<int> s1 = {1, 2, 3};

    auto s2 = Copy(s1);

    for (auto i = 0; i != s1.size(); ++i) {
        ASSERT_EQ(s1[i], s2[i]);
    }
}